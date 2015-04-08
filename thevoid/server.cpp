/*
 * Copyright 2013+ Ruslan Nigmatullin <euroelessar@yandex.ru>
 * Copyright 2013+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "server_p.hpp"
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>

#include <vector>
#include <boost/thread.hpp>
#include <pthread.h>
#include <functional>
#include <iostream>

#include <swarm/url.hpp>
#include <swarm/logger.hpp>
#include <thevoid/rapidjson/filestream.h>

#include <sys/wait.h>

#ifdef __linux__
# include <sys/prctl.h>
#endif

#include <blackhole/repository.hpp>
#include <blackhole/repository/config/parser/rapidjson.hpp>
#include <blackhole/frontend/syslog.hpp>
#include <blackhole/frontend/files.hpp>
#include <blackhole/sink/socket.hpp>

#define UNIX_PREFIX "unix:"
#define UNIX_PREFIX_LEN (sizeof(UNIX_PREFIX) / sizeof(char) - 1)

namespace ioremap {
namespace thevoid {

class server_data;

server_data::server_data(base_server *server) :
	logger(base_logger, blackhole::log::attributes_t()),
	connections_counter(0),
	active_connections_counter(0),
	server(server),
	io_service(new boost::asio::io_service),
	monitor_io_service(new boost::asio::io_service),
	threads_round_robin(0),
	threads_count(2),
	backlog_size(128),
	buffer_size(8192),
	local_acceptors(new acceptors_list<unix_connection>(*this)),
	tcp_acceptors(new acceptors_list<tcp_connection>(*this)),
	monitor_acceptors(new acceptors_list<monitor_connection>(*this)),
	daemonize(false),
	safe_mode(false),
	options_parsed(false)
{
	swarm::utils::logger::init_attributes(base_logger);
}

server_data::~server_data()
{
}

void server_data::handle_stop()
{
	worker_works.clear();
	io_service->stop();
	for (auto it = worker_io_services.begin(); it != worker_io_services.end(); ++it) {
		(*it)->stop();
	}
	monitor_io_service->stop();
}

void server_data::handle_reload()
{
}

boost::asio::io_service &server_data::get_worker_service()
{
	const uint id = (threads_round_robin++ % threads_count);
	return *worker_io_services[id];
}

pid_file::pid_file(const std::string &path) : m_path(path), m_file(NULL)
{
}

pid_file::~pid_file()
{
	remove();
}

bool pid_file::remove_stale()
{
	FILE *file = ::fopen(m_path.c_str(), "r");
	if (file) {
		pid_t pid;
		if (::fscanf(file, "%d", &pid) <= 0) {
			::fclose(file);
			if (::unlink(m_path.c_str()) < 0) {
				return false;
			}
			return true;
		}
		::fclose(file);

		if (::kill(pid, 0) < 0 && errno == ESRCH) {
			if (::unlink(m_path.c_str()) < 0) {
				return false;
			}
		} else {
			return false;
		}
	}
	return true;
}

bool pid_file::open()
{
	m_file = ::fopen(m_path.c_str(), "w");
	if (!m_file) {
		return false;
	}
	return true;
}

void pid_file::write()
{
	::fprintf(m_file, "%d", ::getpid());
	::fclose(m_file);
	m_file = NULL;
}

bool pid_file::remove()
{
	if (::unlink(m_path.c_str()) < 0)
		return false;

	return true;
}

base_server::base_server() : m_data(new server_data(this))
{
}

base_server::~base_server()
{
	m_data->handle_stop();

	m_data->worker_io_services.clear();
	m_data->io_service.reset();
	m_data->monitor_io_service.reset();
}

const swarm::logger &base_server::logger() const
{
	return m_data->logger;
}

std::map<std::string, std::string> base_server::get_statistics() const
{
	return std::map<std::string, std::string>();
}

unsigned int base_server::threads_count() const
{
	return m_data->threads_count;
}

bool base_server::initialize_logger(const rapidjson::Value &config)
{
	if (!config.HasMember("logger")) {
		std::cerr << "Failed to initialize logger: \"logger\" field is missed" << std::endl;
		return false;
	}

	const rapidjson::Value &logger = config["logger"];

	if (!logger.HasMember("frontends")) {
		std::cerr << "Failed to initialize logger: \"logger.frontends\" field is missed" << std::endl;
		return false;
	}

	if (!logger.HasMember("level")) {
		std::cerr << "Failed to initialize logger: \"logger.level\" field is missed" << std::endl;
		return false;
	}

	const rapidjson::Value &frontends = logger["frontends"];

	if (!frontends.IsArray()) {
		std::cerr << "Failed to initialize logger: \"logger.frontends\" field must be an array" << std::endl;
		return false;
	}

	const rapidjson::Value &level = logger["level"];

	if (!level.IsString()) {
		std::cerr << "Failed to initialize logger: \"logger.level\" field must be a string" << std::endl;
		return false;
	}

	using namespace blackhole;

	// Available logging sinks.
	typedef boost::mpl::vector<
	    blackhole::sink::files_t<
			sink::files::boost_backend_t,
			sink::rotator_t<
				sink::files::boost_backend_t,
				sink::rotation::watcher::move_t
			>
		>,
	    blackhole::sink::syslog_t<swarm::log_level>,
	    blackhole::sink::socket_t<boost::asio::ip::tcp>,
	    blackhole::sink::socket_t<boost::asio::ip::udp>
	> sinks_t;

	// Available logging formatters.
	typedef boost::mpl::vector<
	    blackhole::formatter::string_t
//	    blackhole::formatter::json_t
	> formatters_t;

	auto &repository = blackhole::repository_t::instance();
	repository.configure<sinks_t, formatters_t>();

	const dynamic_t &dynamic = repository::config::transformer_t<
		rapidjson::Value
        >::transform(frontends);

	log_config_t log_config = repository::config::parser_t<log_config_t>::parse("root", dynamic);

	const auto mapper = swarm::utils::logger::mapping();
	for(auto it = log_config.frontends.begin(); it != log_config.frontends.end(); ++it) {
		it->formatter.mapper = mapper;
	}

	repository.add_config(log_config);

	m_data->base_logger = repository.root<swarm::log_level>();
	swarm::utils::logger::init_attributes(m_data->base_logger);
	m_data->base_logger.verbosity(swarm::utils::logger::parse_level(level.GetString()));

	return true;
}

void base_server::on(base_server::options &&opts, const std::shared_ptr<base_stream_factory> &factory)
{
	m_data->handlers.emplace_back(std::move(opts), factory);
}

static pid_t start_daemon(pid_file *file)
{
	pid_t pid;

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "Failed to fork to background: %s.\n", strerror(errno));
		throw daemon_exception("failed to fork to background");
	}

	if (pid != 0) {
		printf("Children pid: %d\n", pid);
		return pid;
	}
	setsid();
	if (file)
		file->write();

	return 0;
}

void base_server::daemonize()
{
	if (!m_data->daemonize) {
		return;
	}

	if (!m_data->pid_file_path.empty()) {
		m_data->pid.reset(new pid_file(m_data->pid_file_path));
		if (!m_data->pid->remove_stale()) {
			throw daemon_exception("another process is active");
		}
		if (!m_data->pid->open()) {
			throw daemon_exception("can not open pid file");
		}
	}

	pid_t err = start_daemon(m_data->pid.get());
	if (err > 0) {
		std::_Exit(0);
	} else {
		return;
	}
}

void base_server::listen(const std::string &host)
{
	if (host.compare(0, UNIX_PREFIX_LEN, UNIX_PREFIX) == 0) {
		// Unix socket
		std::string file = host.substr(UNIX_PREFIX_LEN);

		m_data->local_acceptors->add_acceptor(file);
	} else {
		m_data->tcp_acceptors->add_acceptor(host);
    }
}

static int read_config(rapidjson::Document &doc, const char *config_path)
{
	FILE *config_file = fopen(config_path, "r");

	if (!config_file) {
		int err = -errno;
		std::cerr << "Can't open file: \"" << config_path << "\": "
			  << strerror(-err) << " (" << err << ")" << std::endl;
		return -2;
	}

	rapidjson::FileStream config_stream(config_file);

	doc.ParseStream<rapidjson::kParseDefaultFlags>(config_stream);

	fclose(config_file);

	if (doc.HasParseError()) {
		std::cerr << "Parse error: \"" << doc.GetParseError() << "\"" << std::endl;
		return -3;
	}

	return 0;
}

struct io_service_runner
{
	boost::asio::io_service *service;
	const char *name;

	void operator() () const
	{
#ifdef __linux__
		prctl(PR_SET_NAME, name);
#endif
		service->run();
	}
};

int base_server::run(int argc, char **argv)
{
	int err = parse_arguments(argc, argv);
	if (err == 0)
		err = run();
	return err;
}

int base_server::parse_arguments(int argc, char **argv)
{
	if (m_data->options_parsed) {
		std::cerr << "options are already parsed" << std::endl;
		return -9;
	}

	namespace po = boost::program_options;
		po::options_description description("Options");

	std::string config_path;

	description.add_options()
		("help", "this help message")
		("config,c", po::value<std::string>(&config_path), "config path (required)")
		("daemonize,d", "daemonize on start")
		("pidfile,p", po::value<std::string>(&m_data->pid_file_path), "location of a pid file")
	;

	po::variables_map options;
	po::store(po::command_line_parser(argc, argv)
		  .options(description).run(),
		  options);
	try {
		po::notify(options);
	} catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		std::cerr << description << std::endl;
		return -1;
	}

	if (options.count("help") || !options.count("config")) {
		if (!options.count("config"))
			std::cerr << "\"config\" is required" << std::endl;
		std::cerr << description << std::endl;
		return -1;
	}

	rapidjson::Document config;
	int err = read_config(config, config_path.c_str());

	if (err) {
		return err;
	}

	if (!initialize_logger(config)) {
		std::cerr << "Failed to initialize logger" << std::endl;
		return -8;
	}

	if (!config.HasMember("application")) {
		BH_LOG(logger(), SWARM_LOG_ERROR, "\"application\" field is missed");
		return -5;
	}

	if (config.HasMember("safe_mode")) {
		m_data->safe_mode = config["safe_mode"].GetBool();
	}

	if (config.HasMember("request_header")) {
		m_data->request_header = config["request_header"].GetString();
	}

	if (config.HasMember("trace_header")) {
		m_data->trace_header = config["trace_header"].GetString();
	}

	if (options.count("daemonize")) {
		m_data->daemonize = true;
	} else if (config.HasMember("daemon")) {
		const rapidjson::Value &daemon = config["daemon"];

		if (daemon.HasMember("fork")) {
			m_data->daemonize = daemon["fork"].GetBool();

			if (daemon.HasMember("uid")) {
				m_data->user_id = daemon["uid"].GetUint();
			}
		}
	}

	if (config.HasMember("threads")) {
		m_data->threads_count = config["threads"].GetUint();
	}

	try {
		if (!initialize(config["application"])) {
			BH_LOG(logger(), SWARM_LOG_ERROR, "Failed to initialize application");
			return -5;
		}
	} catch (std::exception &exc) {
		std::cerr << "Failed to initialize application: " << exc.what() << std::endl;
		return -5;
	}

	if (!config.HasMember("endpoints")) {
		BH_LOG(logger(), SWARM_LOG_ERROR, "\"endpoints\" field is missed");
		return -4;
	}

	auto &endpoints = config["endpoints"];

	if (!endpoints.IsArray()) {
		BH_LOG(logger(), SWARM_LOG_ERROR, "\"endpoints\" field is not an array");
		return -4;
	}

	if (config.HasMember("buffer_size")) {
		m_data->buffer_size = config["buffer_size"].GetUint();
	}

	if (config.HasMember("backlog")) {
		m_data->backlog_size = config["backlog"].GetInt();
	}

	for (size_t i = 0; i < m_data->threads_count; ++i) {
		m_data->worker_io_services.emplace_back(new boost::asio::io_service(1));
		m_data->worker_works.emplace_back(new boost::asio::io_service::work(*m_data->worker_io_services[i]));
	}

	try {
		for (auto it = endpoints.Begin(); it != endpoints.End(); ++it) {
			listen(it->GetString());
		}
	} catch (...) {
		return -6;
	}

	if (m_data->daemonize && m_data->user_id) {
		int err = setuid(*m_data->user_id);
		if (err == -1) {
			return errno;
		}
	}

	int monitor_port = -1;

	if (config.HasMember("monitor-port")) {
		monitor_port = config["monitor-port"].GetInt();
	}

	try {
		if (monitor_port != -1) {
			m_data->monitor_acceptors->add_acceptor("0.0.0.0:" + boost::lexical_cast<std::string>(monitor_port));
		}
	} catch (...) {
		return -7;
	}

	m_data->options_parsed = true;

	return 0;
}

int base_server::run()
{
	if (!m_data->options_parsed) {
		std::cerr << "options are not parsed" << std::endl;
		return -9;
	}

	m_data->worker_works.emplace_back(new boost::asio::io_service::work(*m_data->monitor_io_service));
	m_data->worker_works.emplace_back(new boost::asio::io_service::work(*m_data->io_service));

	std::vector<std::unique_ptr<boost::thread> > threads;
	io_service_runner runner;
	runner.name = "void_worker";

	for (size_t i = 0; i < m_data->threads_count; ++i) {
		runner.service = m_data->worker_io_services[i].get();
		m_data->worker_threads.emplace_back(new boost::thread(runner));
	}

	runner.name = "void_monitor";
	runner.service = m_data->monitor_io_service.get();
	threads.emplace_back(new boost::thread(runner));

	runner.name = "void_acceptor";
	runner.service = m_data->io_service.get();
	threads.emplace_back(new boost::thread(runner));

	// create signal_service instance to register the server
	// within global signal handling mechanics
	auto sigservice = std::make_shared<signal_service>(m_data->monitor_io_service.get(), this);

	// Wait for all threads in the pool to exit.
	for (std::size_t i = 0; i < threads.size(); ++i)
		threads[i]->join();
	for (std::size_t i = 0; i < m_data->worker_threads.size(); ++i)
		m_data->worker_threads[i]->join();

	// on destruction the server will be deregistered from
	// global signal handling mechanics
	sigservice.reset();

	m_data->local_acceptors.reset();
	m_data->tcp_acceptors.reset();
	m_data->monitor_acceptors.reset();
	m_data->pid.reset();

	return 0;
}

void base_server::stop()
{
	m_data->handle_stop();
}

void base_server::reload()
{
	m_data->handle_reload();
}

std::shared_ptr<base_stream_factory> base_server::factory(const http_request &request)
{
	for (auto it = m_data->handlers.begin(); it != m_data->handlers.end(); ++it) {
		if (it->first.check(request)) {
			return it->second;
		}
	}

	return std::shared_ptr<base_stream_factory>();
}

daemon_exception::daemon_exception() : runtime_error("daemon initialization failed")
{

}

daemon_exception::daemon_exception(const std::string &error) : runtime_error("daemon initialization failed: " + error)
{
}

} } // namespace ioremap::thevoid
