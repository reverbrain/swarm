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

#include "server.hpp"
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <vector>
#include <set>
#include <thread>
#include <functional>
#include <mutex>
#include <iostream>

#include <signal.h>

#include "connection_p.hpp"

#include <swarm/network_url.h>

#define UNIX_PREFIX "unix:"
#define UNIX_PREFIX_LEN (sizeof(UNIX_PREFIX) / sizeof(char) - 1)

namespace ioremap {
namespace thevoid {

template <typename T>
class acceptors_list
{
public:
	typedef typename T::endpoint endpoint;
	typedef boost::asio::basic_socket_acceptor<T> acceptor;
	typedef boost::asio::basic_stream_socket<T> socket;
	typedef ioremap::thevoid::connection<socket> connection;
	typedef std::shared_ptr<connection> connection_ptr;

	acceptors_list(server_data &data) : data(data)
	{
	}

	~acceptors_list() {}

	std::unique_ptr<acceptor> create_acceptor();
	void add_acceptor(std::unique_ptr<acceptor> &&acc);
	void start_acceptor(size_t index);
	void handle_accept(size_t index, const connection_ptr &conn, const boost::system::error_code &err);

	void start_threads(std::vector<std::thread> &threads)
	{
		for (size_t i = 0; i < io_services.size(); ++i) {
			auto functor = boost::bind(&boost::asio::io_service::run, io_services[i].get());
			for (int j = 0; j < 4; ++j) {
				threads.emplace_back(functor);
			}
		}
	}

	void handle_stop()
	{
		for (size_t i = 0; i < io_services.size(); ++i) {
			io_services[i]->stop();
		}
	}

	server_data &data;
	std::vector<std::unique_ptr<acceptor>> acceptors;
	std::vector<std::unique_ptr<boost::asio::io_service>> io_services;
};

template <>
acceptors_list<boost::asio::local::stream_protocol>::~acceptors_list()
{
	for (size_t i = 0; i < acceptors.size(); ++i) {
		auto &acceptor = *acceptors[i];
		auto path = acceptor.local_endpoint().path();
		unlink(path.c_str());
	}
}

class server_data;

//! This handler is created to resolve creation of several servers in one process,
//! all of them must be stopped on SIGINT/SIGTERM signal
class signal_handler
{
public:
	signal_handler()
	{
		if (SIG_ERR == signal(SIGINT, handler)) {
			throw std::runtime_error("Cannot set up SIGINT handler");
		}
		if (SIG_ERR == signal(SIGTERM, handler)) {
			throw std::runtime_error("Cannot set up SIGTERM handler");
		}
	}

	~signal_handler()
	{
	}

	static void handler(int);

	std::mutex lock;
	std::set<server_data*> all_servers;
};

static std::weak_ptr<signal_handler> global_signal_set;

class server_data
{
public:
	server_data() :
		local_acceptors(*this),
		tcp_acceptors(*this),
		signal_set(global_signal_set.lock())
	{
		if (!signal_set) {
			signal_set = std::make_shared<signal_handler>();
			global_signal_set = signal_set;
		}

		std::lock_guard<std::mutex> locker(signal_set->lock);
		signal_set->all_servers.insert(this);
	}

	~server_data()
	{
		std::lock_guard<std::mutex> locker(signal_set->lock);
		signal_set->all_servers.erase(this);
	}

	void handle_stop()
	{
		local_acceptors.handle_stop();
		tcp_acceptors.handle_stop();
		io_service.stop();
	}

	//! Weak pointer to server itself
	std::weak_ptr<base_server> server;
	//! The io_service used to perform asynchronous operations.
	boost::asio::io_service io_service;
	//! Size of thread pool
	unsigned int threads_count;
	//! List of activated acceptors
	acceptors_list<boost::asio::local::stream_protocol> local_acceptors;
	acceptors_list<boost::asio::ip::tcp> tcp_acceptors;
	//! The signal_set is used to register for process termination notifications.
	std::shared_ptr<signal_handler> signal_set;
	//! User handlers for urls
	std::map<std::string, std::shared_ptr<base_stream_factory>> handlers;
};

void signal_handler::handler(int)
{
	if (auto signal_set = global_signal_set.lock()) {
		std::lock_guard<std::mutex> locker(signal_set->lock);

		for (auto it = signal_set->all_servers.begin(); it != signal_set->all_servers.end(); ++it)
			(*it)->handle_stop();
	}
}

template <typename T>
std::unique_ptr<boost::asio::basic_socket_acceptor<T> > acceptors_list<T>::create_acceptor()
{
//	std::unique_ptr<boost::asio::io_service> io_service(new boost::asio::io_service);
	io_services.emplace_back(new boost::asio::io_service);
	return std::unique_ptr<acceptor>(new acceptor(*io_services.back()));
}

template <typename T>
void acceptors_list<T>::add_acceptor(std::unique_ptr<acceptor> &&acc)
{
	acceptors.emplace_back(std::move(acc));
	start_acceptor(acceptors.size() - 1);
}

template <typename T>
void acceptors_list<T>::start_acceptor(size_t index)
{
	acceptor &acc = *acceptors[index];

	auto conn = std::make_shared<connection>(acc.get_io_service());

	acc.async_accept(conn->socket(), boost::bind(
		&acceptors_list::handle_accept, this, index, conn, _1));
}

template <typename T>
void acceptors_list<T>::handle_accept(size_t index, const connection_ptr &conn, const boost::system::error_code &err)
{
	if (!err) {
		if (auto server = data.server.lock()) {
			conn->start(server);
		} else {
			throw std::logic_error("server::m_data->server is null");
		}
	}

	start_acceptor(index);
}

base_server::base_server() : m_data(new server_data)
{
	m_data->threads_count = std::thread::hardware_concurrency();
}

base_server::~base_server()
{
	delete m_data;
}

enum { MAX_CONNECTIONS_COUNT = 128 };

void base_server::listen(const std::string &host)
{
	if (host.compare(0, UNIX_PREFIX_LEN, UNIX_PREFIX) == 0) {
		// Unix socket
		std::string file = host.substr(UNIX_PREFIX_LEN);

		std::cerr << file << std::endl;

		auto acceptor = m_data->local_acceptors.create_acceptor();
		boost::asio::local::stream_protocol::endpoint endpoint(file);
		acceptor->open(endpoint.protocol());
		acceptor->set_option(boost::asio::local::stream_protocol::acceptor::reuse_address(true));
		acceptor->bind(endpoint);
		acceptor->listen(MAX_CONNECTIONS_COUNT);

		m_data->local_acceptors.add_acceptor(std::move(acceptor));
	} else {
		// TCP socket
		size_t delim = host.find(':');
		std::string address = host.substr(0, delim);
		std::string port = host.substr(delim + 1);

		// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
		auto acceptor = m_data->tcp_acceptors.create_acceptor();
		boost::asio::ip::tcp::resolver resolver(acceptor->get_io_service());
		boost::asio::ip::tcp::resolver::query query(address, port);
		boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);
		acceptor->open(endpoint.protocol());
		acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
		acceptor->bind(endpoint);
		acceptor->listen(MAX_CONNECTIONS_COUNT);

		m_data->tcp_acceptors.add_acceptor(std::move(acceptor));
	}
}

void base_server::run()
{
	std::vector<std::thread> threads;

	m_data->local_acceptors.start_threads(threads);
	m_data->tcp_acceptors.start_threads(threads);

	// Wait for all threads in the pool to exit.
	for (std::size_t i = 0; i < threads.size(); ++i)
		threads[i].join();
}

void base_server::initialize()
{
}

void base_server::on(const std::string &url, const std::shared_ptr<base_stream_factory> &factory)
{
	m_data->handlers[url] = factory;
}

void base_server::set_server(const std::weak_ptr<base_server> &server)
{
	m_data->server = server;
}

std::shared_ptr<base_stream_factory> base_server::get_factory(const std::string &url)
{
    swarm::network_url url_parser;
    url_parser.set_base(url);
    return m_data->handlers[url_parser.path()];
}


} } // namespace ioremap::thevoid
