/*
 * Copyright 2013+ Ruslan Nigmatullin <euroelessar@yandex.ru>
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
#include <functional>
#include <iostream>

#include <signal.h>

#include <swarm/url.hpp>
#include <swarm/logger.hpp>
#include <thevoid/rapidjson/filereadstream.h>

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
	local_acceptors(*this),
	tcp_acceptors(*this),
	monitor_acceptors(*this),
	signal_set(global_signal_set.lock())
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

boost::asio::io_service &server_data::get_worker_service()
{
	const uint id = (threads_round_robin++ % threads_count);
	return *worker_io_services[id];
}

void signal_handler::handler(int)
{
	if (auto signal_set = global_signal_set.lock()) {
		std::lock_guard<std::mutex> locker(signal_set->lock);

		for (auto it = signal_set->all_servers.begin(); it != signal_set->all_servers.end(); ++it)
			(*it)->handle_stop();
	}
}

base_server::base_server() : m_data(new server_data)
{
	m_data->threads_count = 2;
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
		set_logger(swarm::logger("/dev/stderr", swarm::LOG_INFO));
		logger().log(swarm::LOG_ERROR, "\"logger\" field is missed, use default logger");
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
		int level = swarm::LOG_INFO;

		if (logger_config.HasMember("file"))
			file = logger_config["file"].GetString();

		if (logger_config.HasMember("level"))
			level = logger_config["level"].GetInt();

		set_logger(swarm::logger(file, level));
	} else {
		set_logger(swarm::logger("/dev/stderr", swarm::LOG_INFO));
		logger().log(swarm::LOG_ERROR, "unknown logger type \"%s\", use default, possible values are: file", type.c_str());
	}
	return true;
}

void base_server::on(base_server::options &&opts, const std::shared_ptr<base_stream_factory> &factory)
{
	m_data->handlers.emplace_back(std::move(opts), factory);
}

void base_server::listen(const std::string &host)
{
	if (host.compare(0, UNIX_PREFIX_LEN, UNIX_PREFIX) == 0) {
		// Unix socket
		std::string file = host.substr(UNIX_PREFIX_LEN);

		m_data->local_acceptors.add_acceptor(file);
	} else {
		m_data->tcp_acceptors.add_acceptor(host);
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

	char buffer[8 * 1024];

	rapidjson::FileReadStream config_stream(config_file, buffer, sizeof(buffer));

	doc.ParseStream<rapidjson::kParseDefaultFlags, rapidjson::UTF8<> >(config_stream);

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
	namespace po = boost::program_options;
		po::options_description description("Options");

	std::string config_path;

	description.add_options()
		("help", "this help message")
		("config,c", po::value<std::string>(&config_path), "config path (required)")
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
		logger().log(swarm::LOG_ERROR, "\"application\" field is missed");
		return -5;
	}

	if (config.HasMember("threads")) {
		m_data->threads_count = config["threads"].GetUint();
	}

	try {
		if (!initialize(config["application"])) {
			logger().log(swarm::LOG_ERROR, "Failed to initialize application");
			return -5;
		}
	} catch (std::exception &exc) {
		std::cerr << "Failed to initialize application: " << exc.what() << std::endl;
		return -5;
	}

	auto endpoints = config.FindMember("endpoints");

	if (!endpoints) {
		logger().log(swarm::LOG_ERROR, "\"endpoints\" field is missed");
		return -4;
	}

	if (!endpoints->value.IsArray()) {
		logger().log(swarm::LOG_ERROR, "\"endpoints\" field is not an array");
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
		for (auto it = endpoints->value.Begin(); it != endpoints->value.End(); ++it) {
			listen(it->GetString());
		}
	} catch (...) {
		return -6;
	}

	int monitor_port = -1;

	if (config.HasMember("daemon")) {
		auto &daemon = config["daemon"];

		if (daemon.HasMember("monitor-port")) {
			monitor_port = daemon["monitor-port"].GetInt();
		}
	}

	if (config.HasMember("backlog"))
		m_data->backlog_size = config["backlog"].GetInt();

	try {
		if (monitor_port != -1) {
			m_data->monitor_acceptors.add_acceptor("0.0.0.0:" + boost::lexical_cast<std::string>(monitor_port));
		}
	} catch (...) {
		return -7;
	}

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

	// Wait for all threads in the pool to exit.
	for (std::size_t i = 0; i < threads.size(); ++i)
		threads[i]->join();
	for (std::size_t i = 0; i < m_data->worker_threads.size(); ++i)
		m_data->worker_threads[i]->join();

	return 0;
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
		check_methods           = 0x01,
		check_exact_match       = 0x02,
		check_prefix_match      = 0x04,
		check_string_match      = 0x08,
		check_regexp_match      = 0x10,
		check_all_match         = check_exact_match | check_prefix_match | check_string_match | check_regexp_match
	};

	server_options_private() : flags(0)
	{
	}

	uint64_t flags;
	std::string match_string;
	std::vector<std::string> methods;
};

base_server::options::options() : m_data(new server_options_private)
{
}

base_server::options::options(options &&other) : m_data(std::move(other.m_data))
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

void base_server::options::set_methods(const std::vector<std::string> &methods)
{
	m_data->flags |= server_options_private::check_methods;
	m_data->methods = methods;
}

void base_server::options::set_methods(base_server::options::methods::special_value value)
{
	if (value == methods::all) {
		m_data->flags &= ~server_options_private::check_methods;
	} else {
		throw std::runtime_error("unknown options::methods::special_value: " + boost::lexical_cast<std::string>(value));
	}
}

bool base_server::options::check(const swarm::http_request &request) const
{
	if (m_data->flags & server_options_private::check_methods) {
		const auto &methods = m_data->methods;
		if (std::find(methods.begin(), methods.end(), request.method()) == methods.end())
			return false;
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
		}
	}

	return true;
}

void base_server::options::swap(base_server::options &other)
{
	using std::swap;
	swap(m_data, other.m_data);
}

} } // namespace ioremap::thevoid
