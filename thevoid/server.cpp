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
#include <boost/regex.hpp>
#include <pthread.h>
#include <functional>
#include <iostream>

#include <signal.h>

#include <swarm/url.hpp>
#include <swarm/logger.hpp>
#include <thevoid/rapidjson/filestream.h>

#include <sys/wait.h>

#ifdef __linux__
# include <sys/prctl.h>
#endif

#define UNIX_PREFIX "unix:"
#define UNIX_PREFIX_LEN (sizeof(UNIX_PREFIX) / sizeof(char) - 1)

namespace ioremap {
namespace thevoid {

class server_data;

static std::weak_ptr<signal_handler> global_signal_set;

server_data::server_data() :
	connections_counter(0),
	active_connections_counter(0),
	threads_round_robin(0),
	threads_count(2),
	backlog_size(128),
	buffer_size(8192),
	local_acceptors(new acceptors_list<unix_connection>(*this)),
	tcp_acceptors(new acceptors_list<tcp_connection>(*this)),
	monitor_acceptors(new acceptors_list<monitor_connection>(*this)),
	signal_set(global_signal_set.lock()),
	daemonize(false),
	safe_mode(false),
	options_parsed(false)
{
	if (!signal_set) {
		signal_set = std::make_shared<signal_handler>();
		global_signal_set = signal_set;
	}

	std::lock_guard<std::mutex> locker(signal_set->lock);
	signal_set->all_servers.insert(this);
}

server_data::~server_data()
{
	std::lock_guard<std::mutex> locker(signal_set->lock);
	signal_set->all_servers.erase(this);
}

void server_data::handle_stop()
{
	worker_works.clear();
	io_service.stop();
	for (auto it = worker_io_services.begin(); it != worker_io_services.end(); ++it) {
		(*it)->stop();
	}
	monitor_io_service.stop();
}

void server_data::handle_reload()
{
	logger.reopen();
}

boost::asio::io_service &server_data::get_worker_service()
{
	const uint id = (threads_round_robin++ % threads_count);
	return *worker_io_services[id];
}

void signal_handler::stop_handler(int signal_value)
{
	if (auto signal_set = global_signal_set.lock()) {
		std::lock_guard<std::mutex> locker(signal_set->lock);

		for (auto it = signal_set->all_servers.begin(); it != signal_set->all_servers.end(); ++it) {
			(*it)->logger.log(swarm::SWARM_LOG_INFO, "Handled signal [%d], stop server", signal_value);
			(*it)->handle_stop();
		}
	}
}

void signal_handler::reload_handler(int signal_value)
{
	if (auto signal_set = global_signal_set.lock()) {
		std::lock_guard<std::mutex> locker(signal_set->lock);

		for (auto it = signal_set->all_servers.begin(); it != signal_set->all_servers.end(); ++it) {
			(*it)->logger.log(swarm::SWARM_LOG_INFO, "Handled signal [%d], reload configuration", signal_value);
			try {
				(*it)->handle_reload();
			} catch (std::exception &e) {
				std::fprintf(stderr, "Failed to reload configuration: %s", e.what());
			}
		}
	}
}

void signal_handler::ignore_handler(int signal_value)
{
	if (auto signal_set = global_signal_set.lock()) {
		std::lock_guard<std::mutex> locker(signal_set->lock);

		for (auto it = signal_set->all_servers.begin(); it != signal_set->all_servers.end(); ++it) {
			(*it)->logger.log(swarm::SWARM_LOG_INFO, "Handled signal [%d], ignored", signal_value);
		}
	}
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

base_server::base_server() : m_data(new server_data)
{
}

base_server::~base_server()
{
}

void base_server::set_logger(const swarm::logger &logger)
{
	m_data->logger = logger;
}

swarm::logger base_server::logger() const
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
		set_logger(swarm::logger("/dev/stderr", swarm::SWARM_LOG_INFO));
		logger().log(swarm::SWARM_LOG_ERROR, "\"logger\" field is missed, use default logger");
		return true;
	}

	const rapidjson::Value &logger_config = config["logger"];

	std::string type;
	if (logger_config.HasMember("type")) {
		type = logger_config["type"].GetString();
	} else {
		type = "file";
	}

	if (type == "file") {
		const char *file = "/dev/stderr";
		int level = swarm::SWARM_LOG_INFO;

		if (logger_config.HasMember("file"))
			file = logger_config["file"].GetString();

		if (logger_config.HasMember("level"))
			level = logger_config["level"].GetInt();

		set_logger(swarm::logger(file, level));
	} else {
		set_logger(swarm::logger("/dev/stderr", swarm::SWARM_LOG_INFO));
		logger().log(swarm::SWARM_LOG_ERROR, "unknown logger type \"%s\", use default, possible values are: file", type.c_str());
	}
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
		logger().log(swarm::SWARM_LOG_ERROR, "\"application\" field is missed");
		return -5;
	}

	if (config.HasMember("safe_mode")) {
		m_data->safe_mode = config["safe_mode"].GetBool();
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
			logger().log(swarm::SWARM_LOG_ERROR, "Failed to initialize application");
			return -5;
		}
	} catch (std::exception &exc) {
		std::cerr << "Failed to initialize application: " << exc.what() << std::endl;
		return -5;
	}

	if (!config.HasMember("endpoints")) {
		logger().log(swarm::SWARM_LOG_ERROR, "\"endpoints\" field is missed");
		return -4;
	}

	auto &endpoints = config["endpoints"];

	if (!endpoints.IsArray()) {
		logger().log(swarm::SWARM_LOG_ERROR, "\"endpoints\" field is not an array");
		return -4;
	}

	if (config.HasMember("buffer_size")) {
		m_data->buffer_size = config["buffer_size"].GetUint();
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

	if (config.HasMember("backlog"))
		m_data->backlog_size = config["backlog"].GetInt();

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

	sigset_t previous_sigset;
	sigset_t sigset;
	sigfillset(&sigset);
	pthread_sigmask(SIG_BLOCK, &sigset, &previous_sigset);

	m_data->worker_works.emplace_back(new boost::asio::io_service::work(m_data->monitor_io_service));
	m_data->worker_works.emplace_back(new boost::asio::io_service::work(m_data->io_service));

	std::vector<std::unique_ptr<boost::thread> > threads;
	io_service_runner runner;
	runner.name = "void_worker";

	for (size_t i = 0; i < m_data->threads_count; ++i) {
		runner.service = m_data->worker_io_services[i].get();
		m_data->worker_threads.emplace_back(new boost::thread(runner));
	}

	runner.name = "void_monitor";
	runner.service = &m_data->monitor_io_service;
	threads.emplace_back(new boost::thread(runner));

	runner.name = "void_acceptor";
	runner.service = &m_data->io_service;
	threads.emplace_back(new boost::thread(runner));

	pthread_sigmask(SIG_SETMASK, &previous_sigset, NULL);

	// Wait for all threads in the pool to exit.
	for (std::size_t i = 0; i < threads.size(); ++i)
		threads[i]->join();
	for (std::size_t i = 0; i < m_data->worker_threads.size(); ++i)
		m_data->worker_threads[i]->join();

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

void base_server::set_server(const std::weak_ptr<base_server> &server)
{
	m_data->server = server;
}

std::shared_ptr<base_stream_factory> base_server::factory(const swarm::http_request &request)
{
	for (auto it = m_data->handlers.begin(); it != m_data->handlers.end(); ++it) {
		if (it->first.check(request)) {
			return it->second;
		}
	}

	return std::shared_ptr<base_stream_factory>();
}

class server_options_private
{
public:
	enum flag : uint64_t {
		check_nothing           = 0x00,
		check_methods           = 0x01,
		check_exact_match       = 0x02,
		check_prefix_match      = 0x04,
		check_string_match      = 0x08,
		check_regexp_match      = 0x10,
		check_headers           = 0x20,
		check_all_match         = check_exact_match | check_prefix_match | check_string_match | check_regexp_match,
		check_min_path_components   = 0x0040,
		check_exact_path_components = 0x0080,
		check_max_path_components   = 0x0100,
		check_all_path_components   = check_min_path_components | check_exact_path_components | check_max_path_components,
		check_host_suffix           = 0x0200,
		check_host_exact            = 0x0400,
		check_host_all              = check_host_suffix | check_host_exact,
		check_query                 = 0x0800
	};

	server_options_private() : flags(check_nothing), path_components_count(0)
	{
	}

	uint64_t flags;
	std::string match_string;
	boost::regex match_regex;
	std::vector<std::string> methods;
	std::vector<swarm::headers_entry> headers;
	std::string host_string;
	size_t path_components_count;
	std::vector<std::pair<std::string, boost::optional<std::string>>> queries;
};

base_server::options::modificator base_server::options::exact_match(const std::string &str)
{
	return std::bind(&base_server::options::set_exact_match, std::placeholders::_1, str);
}

base_server::options::modificator base_server::options::prefix_match(const std::string &str)
{
	return std::bind(&base_server::options::set_prefix_match, std::placeholders::_1, str);
}

base_server::options::modificator base_server::options::regex_match(const std::string &str)
{
	return std::bind(&base_server::options::set_regex_match, std::placeholders::_1, str);
}

base_server::options::modificator base_server::options::methods(const std::vector<std::string> &methods)
{
	return std::bind(&base_server::options::set_methods, std::placeholders::_1, methods);
}

base_server::options::modificator base_server::options::header(const std::string &name, const std::string &value)
{
	return std::bind(&base_server::options::set_header, std::placeholders::_1, name, value);
}

base_server::options::modificator base_server::options::minimal_path_components_count(size_t count)
{
	return std::bind(&base_server::options::set_minimal_path_components_count, std::placeholders::_1, count);
}

base_server::options::modificator base_server::options::exact_path_components_count(size_t count)
{
	return std::bind(&base_server::options::set_exact_path_components_count, std::placeholders::_1, count);
}

base_server::options::modificator base_server::options::maximal_path_components_count(size_t count)
{
	return std::bind(&base_server::options::set_maximal_path_components_count, std::placeholders::_1, count);
}

base_server::options::modificator base_server::options::query(const std::string &key)
{
	return std::bind(static_cast<void (base_server::options::*)(const std::string &)>(&base_server::options::set_query), std::placeholders::_1, key);
}

base_server::options::modificator base_server::options::query(const std::string &key, const std::string &value)
{
	return std::bind(static_cast<void (base_server::options::*)(const std::string &, const std::string &)>(&base_server::options::set_query), std::placeholders::_1, key, value);
}

base_server::options::modificator base_server::options::host_exact(const std::string &host)
{
	return std::bind(&base_server::options::set_host_exact, std::placeholders::_1, host);
}

base_server::options::modificator base_server::options::host_suffix(const std::string &host)
{
	return std::bind(&base_server::options::set_host_suffix, std::placeholders::_1, host);
}

base_server::options::options() : m_data(new server_options_private)
{
}

base_server::options::options(options &&other) SWARM_NOEXCEPT : m_data(std::move(other.m_data))
{
}

base_server::options &base_server::options::operator =(options &&other)
{
	m_data = std::move(other.m_data);
	return *this;
}

base_server::options::~options()
{
}

void base_server::options::set_exact_match(const std::string &str)
{
	if (m_data->flags & server_options_private::check_all_match) {
		throw std::runtime_error("trying to set_exact_match(" + str + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_exact_match | server_options_private::check_string_match;
	m_data->match_string = str;
}

void base_server::options::set_prefix_match(const std::string &str)
{
	if (m_data->flags & server_options_private::check_all_match) {
		throw std::runtime_error("trying to set_prefix_match(" + str + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_prefix_match | server_options_private::check_string_match;
	m_data->match_string = str;
}

void base_server::options::set_regex_match(const std::string &str)
{
	if (m_data->flags & server_options_private::check_all_match) {
		throw std::runtime_error("trying to set_regex_match(" + str + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_regexp_match | server_options_private::check_string_match;
	m_data->match_regex.assign(str);
}

void base_server::options::set_methods(const std::vector<std::string> &methods)
{
	m_data->flags |= server_options_private::check_methods;
	m_data->methods = methods;
}

void base_server::options::set_header(const std::string &name, const std::string &value)
{
	m_data->flags |= server_options_private::check_headers;
	m_data->headers.emplace_back(name, value);
}

void base_server::options::set_minimal_path_components_count(size_t count)
{
	if (m_data->flags & server_options_private::check_all_path_components) {
		throw std::runtime_error("trying to set_minimal_path_components_count, while another was already set");
	}
	m_data->flags |= server_options_private::check_min_path_components;
	m_data->path_components_count = count;
}

void base_server::options::set_exact_path_components_count(size_t count)
{
	if (m_data->flags & server_options_private::check_all_path_components) {
		throw std::runtime_error("trying to set_exact_path_components_count, while another was already set");
	}
	m_data->flags |= server_options_private::check_exact_path_components;
	m_data->path_components_count = count;
}

void base_server::options::set_maximal_path_components_count(size_t count)
{
	if (m_data->flags & server_options_private::check_all_path_components) {
		throw std::runtime_error("trying to set_maximal_path_components_count, while another was already set");
	}
	m_data->flags |= server_options_private::check_max_path_components;
	m_data->path_components_count = count;
}

void base_server::options::set_query(const std::string &key)
{
	m_data->flags |= server_options_private::check_query;
	m_data->queries.emplace_back(key, boost::none);
}

void base_server::options::set_query(const std::string &key, const std::string &value)
{
	m_data->flags |= server_options_private::check_query;
	m_data->queries.emplace_back(key, value);
}

void base_server::options::set_host_exact(const std::string &host)
{
	if (m_data->flags & server_options_private::check_host_all) {
		throw std::runtime_error("trying to set_host_exact(" + host + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_host_exact;
	m_data->host_string = host;
}

void base_server::options::set_host_suffix(const std::string &host)
{
	if (m_data->flags & server_options_private::check_host_all) {
		throw std::runtime_error("trying to set_host_suffix(" + host + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_host_suffix;
	m_data->host_string = host;
}

bool base_server::options::check(const swarm::http_request &request) const
{
	if (m_data->flags & server_options_private::check_methods) {
		const auto &methods = m_data->methods;
		if (std::find(methods.begin(), methods.end(), request.method()) == methods.end())
			return false;
	}

	if (m_data->flags & server_options_private::check_all_path_components) {
		const size_t path_components_count = request.url().path_components().size();

		switch (m_data->flags & server_options_private::check_all_path_components) {
			case server_options_private::check_min_path_components:
				if (path_components_count < m_data->path_components_count)
					return false;
				break;
			case server_options_private::check_exact_path_components:
				if (path_components_count != m_data->path_components_count)
					return false;
				break;
			case server_options_private::check_max_path_components:
				if (path_components_count > m_data->path_components_count)
					return false;
				break;
			default:
				break;
		}
	}

	if (m_data->flags & server_options_private::check_string_match) {
		const std::string &match = m_data->match_string;

		if (m_data->flags & server_options_private::check_exact_match) {
			if (match != request.url().path()) {
				return false;
			}
		} else if (m_data->flags & server_options_private::check_prefix_match) {
			if (request.url().path().compare(0, match.size(), match) != 0) {
				return false;
			}
		} else if (m_data->flags & server_options_private::check_regexp_match) {
			if (!boost::regex_match(request.url().path(), m_data->match_regex)) {
				return false;
			}
		}
	}

	if (m_data->flags & server_options_private::check_host_all) {
		const auto &host_ptr = request.headers().get("Host");
		if (!host_ptr) {
			return false;
		}
		const std::string &host = *host_ptr;
		// Remove port from 'Host: domain.com:8080'
		const size_t host_size = std::min(host.size(), host.find_first_of(':'));

		if (m_data->flags & server_options_private::check_host_exact) {
			if (host.compare(0, host_size, m_data->host_string) != 0) {
				return false;
			}
		} else if (m_data->flags & server_options_private::check_host_suffix) {
			if (host_size < m_data->host_string.size()) {
				return false;
			}
			if (host.compare(host_size - m_data->host_string.size(), m_data->host_string.size(), m_data->host_string) != 0) {
				return false;
			}
		}
	}

	if (m_data->flags & server_options_private::check_query) {
		const auto &query = request.url().query();
		const auto &queries = m_data->queries;
		for (auto it = queries.begin(); it != queries.end(); ++it) {
			auto value = query.item_value(it->first);
			if (!value) {
				return false;
			}

			if (!it->second) {
				// We just want to check if such query parameter exists, we don't care about the exact value
				continue;
			}

			if (*it->second != *value) {
				// Value mismatch
				return false;
			}
		}
	}

	if (m_data->flags & server_options_private::check_headers) {
		const auto &request_headers = request.headers();
		const auto &headers = m_data->headers;
		for (auto it = headers.begin(); it != headers.end(); ++it) {
			if (auto value = request_headers.get(it->first)) {
				if (*value != it->second) {
					return false;
				}
			} else {
				return false;
			}
		}
	}

	return true;
}

void base_server::options::swap(base_server::options &other)
{
	using std::swap;
	swap(m_data, other.m_data);
}

daemon_exception::daemon_exception() : runtime_error("daemon initialization failed")
{

}

daemon_exception::daemon_exception(const std::string &error) : runtime_error("daemon initialization failed: " + error)
{
}

} } // namespace ioremap::thevoid
