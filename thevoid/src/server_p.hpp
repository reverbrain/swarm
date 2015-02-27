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

#include <mutex>
#include <set>

#include <swarm/logger.hpp>
#include <blackhole/utils/atomic.hpp>

namespace ioremap {
namespace thevoid {

//! This handler is created to resolve creation of several servers in one process,
//! all of them must be stopped on SIGINT/SIGTERM signal
class pid_file
{
public:
	pid_file(const std::string &path);
	~pid_file();

	bool remove_stale();
	bool open();
	void write();
	bool remove();

private:
	std::string m_path;
	FILE *m_file;
};

typedef std::shared_ptr<base_stream_factory> factory_ptr;

class server_data
{
public:
	server_data(base_server *server);

	~server_data();

	void handle_stop();
	void handle_reload();

	boost::asio::io_service &get_worker_service();

	//! Logger instance
	swarm::logger_base base_logger;
	swarm::logger logger;
	//! Statistics
	std::atomic_int connections_counter;
	std::atomic_int active_connections_counter;
	//! Raw pointer to server itself
	base_server *server;
	//! The io_service used to handle new sockets.
	std::unique_ptr<boost::asio::io_service> io_service;
	//! The io_service used to process monitoring connection.
	std::unique_ptr<boost::asio::io_service> monitor_io_service;
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
	std::unique_ptr<acceptors_list<unix_connection>> local_acceptors;
	std::unique_ptr<acceptors_list<tcp_connection>> tcp_acceptors;
	std::unique_ptr<acceptors_list<monitor_connection>> monitor_acceptors;
	//! User handlers for urls
	std::vector<std::pair<base_server::options, factory_ptr>> handlers;
	//! User id change to during deamonization
	boost::optional<uid_t> user_id;
	bool daemonize;
	//! Safe mode
	bool safe_mode;
	bool options_parsed;
	std::unique_ptr<pid_file> pid;
	std::string pid_file_path;

	//! Request ID/Trace bit
	std::string request_header;
	std::string trace_header;
};

}}

#endif // IOREMAP_THEVOID_SERVER_P_HPP
