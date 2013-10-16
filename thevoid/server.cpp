/*
 * 2013+ Copyright (c) Ruslan Nigatullin <euroelessar@yandex.ru>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

#include <swarm/network_url.h>
#include <swarm/logger.h>
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
	delete m_data;
}

void base_server::set_logger(const swarm::logger &logger)
{
	m_data->logger = logger;
}

swarm::logger base_server::get_logger() const
{
	return m_data->logger;
}

void base_server::set_statisitcs_handler(const statistics_fuction &handler)
{
	m_data->statistics_handler = handler;
}

std::map<std::string, std::string> base_server::get_statistics()
{
	if (m_data->statistics_handler)
		return m_data->statistics_handler();
	return std::map<std::string, std::string>();
}

unsigned int base_server::get_threads_count() const
{
	return m_data->threads_count;
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

	if (err)
		return err;

	if (!config.HasMember("application")) {
		std::cerr << "\"application\" field is missed" << std::endl;
		return -5;
	}

	if (config.HasMember("threads")) {
		m_data->threads_count = config["threads"].GetUint();
	}

	try {
		if (!initialize(config.FindMember("application")->value)) {
			std::cerr << "Failed to initialize application" << std::endl;
			return -5;
		}
	} catch (std::exception &exc) {
		std::cerr << "Failed to initialize application: " << exc.what() << std::endl;
		return -5;
	}

	auto endpoints = config.FindMember("endpoints");

	if (!endpoints) {
		std::cerr << "\"endpoints\" field is missed" << std::endl;
		return -4;
	}

	if (!endpoints->value.IsArray()) {
		std::cerr << "\"endpoints\" field is not an array" << std::endl;
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

void base_server::on(const std::string &url, const std::shared_ptr<base_stream_factory> &factory)
{
	if (m_data->handlers.find(url) != m_data->handlers.end()) {
		throw std::logic_error("Handler \"" + url + "\" is already registered");
	}

	m_data->handlers[url] = factory;
}

void base_server::on_prefix(const std::string &url, const std::shared_ptr<base_stream_factory> &factory)
{
	for (auto it = m_data->prefix_handlers.begin(); it != m_data->prefix_handlers.end(); ++it) {
		if (it->first == url) {
			throw std::logic_error("Prefix handler \"" + url + "\" is already registered");
		} else if (url.compare(0, it->first.size(), it->first) == 0) {
			throw std::logic_error("Prefix handler \"" + url + "\" is not accessable because \"" + it->first + "\" already registered");
		}
	}
	m_data->prefix_handlers.emplace_back(url, factory);
}

void base_server::set_server(const std::weak_ptr<base_server> &server)
{
	m_data->server = server;
}

std::shared_ptr<base_stream_factory> base_server::get_factory(const std::string &url)
{
	swarm::network_url url_parser;
	url_parser.set_base(url);
	const std::string path = url_parser.path();

	auto it = m_data->handlers.find(path);

	if (it != m_data->handlers.end())
		return it->second;

	for (auto jt = m_data->prefix_handlers.begin(); jt != m_data->prefix_handlers.end(); ++jt) {
		if (path.compare(0, jt->first.size(), jt->first) == 0) {
			return jt->second;
		}
	}

	return std::shared_ptr<base_stream_factory>();
}


} } // namespace ioremap::thevoid
