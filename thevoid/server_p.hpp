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

#ifndef IOREMAP_THEVOID_SERVER_P_HPP
#define IOREMAP_THEVOID_SERVER_P_HPP

#include "server.hpp"
#include "acceptorlist_p.hpp"
#include "connection_p.hpp"
#include "monitor_connection_p.hpp"
#include <signal.h>

#include <mutex>
#include <set>

#include <swarm/logger.hpp>

#if __GNUC__ == 4 && __GNUC_MINOR__ < 5
#  include <cstdatomic>
#else
#  include <atomic>
#endif

namespace ioremap {
namespace thevoid {

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

typedef std::shared_ptr<base_stream_factory> factory_ptr;

class server_data
{
public:
	server_data();

	~server_data();

	void handle_stop();
    
    boost::asio::io_service &get_worker_service();

    //! Logger instance
    swarm::logger logger;
    //! Statistics
    std::atomic_int connections_counter;
    std::atomic_int active_connections_counter;
	//! Weak pointer to server itself
	std::weak_ptr<base_server> server;
	//! The io_service used to handle new sockets.
	boost::asio::io_service io_service;
    //! The io_service used to process monitoring connection.
	boost::asio::io_service monitor_io_service;
    //! List of io_services to process connections.
    std::vector<std::unique_ptr<boost::asio::io_service>> worker_io_services;
    std::vector<std::unique_ptr<boost::asio::io_service::work>> worker_works;
    std::vector<std::unique_ptr<boost::thread>> worker_threads;
	//! Size of workers thread pool
    std::atomic_uint threads_round_robin;
	unsigned int threads_count;
	unsigned int backlog_size;
    size_t buffer_size;
	//! List of activated acceptors
	acceptors_list<unix_connection> local_acceptors;
	acceptors_list<tcp_connection> tcp_acceptors;
	acceptors_list<monitor_connection> monitor_acceptors;
	//! The signal_set is used to register for process termination notifications.
	std::shared_ptr<signal_handler> signal_set;
	//! User handlers for urls
	std::vector<std::pair<base_server::options, factory_ptr>> handlers;
};

}}

#endif // IOREMAP_THEVOID_SERVER_P_HPP
