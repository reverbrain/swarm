#ifndef IOREMAP_THEVOID_SERVER_P_HPP
#define IOREMAP_THEVOID_SERVER_P_HPP

#include "server.hpp"
#include "acceptorlist_p.hpp"
#include "connection_p.hpp"
#include "monitor_connection_p.hpp"
#include <signal.h>

#include <mutex>
#include <set>

namespace ioremap {
namespace thevoid {

int get_connections_counter();

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

class server_data
{
public:
	server_data();

	~server_data();

	void handle_stop();

	//! Weak pointer to server itself
	std::weak_ptr<base_server> server;
	//! The io_service used to perform asynchronous operations.
	boost::asio::io_service io_service;
	//! Size of thread pool per socket
	unsigned int threads_count;
	unsigned int backlog_size;
	//! List of activated acceptors
	acceptors_list<unix_connection> local_acceptors;
	acceptors_list<tcp_connection> tcp_acceptors;
	acceptors_list<monitor_connection> monitor_acceptors;
	//! The signal_set is used to register for process termination notifications.
	std::shared_ptr<signal_handler> signal_set;
	//! User handlers for urls
	std::vector<std::pair<std::string, std::shared_ptr<base_stream_factory>>> handlers;
};

}}

#endif // IOREMAP_THEVOID_SERVER_P_HPP
