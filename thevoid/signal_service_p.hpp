#ifndef IOREMAP_THEVOID_SIGNAL_SERVICE_P_HPP
#define IOREMAP_THEVOID_SIGNAL_SERVICE_P_HPP

#include <signal.h>

#include <mutex>
#include <set>
#include <thread>

#include <boost/asio.hpp>

#include <thevoid/server.hpp>

namespace ioremap { namespace thevoid {

/*!
 * \brief The signal_service_state class represents global signal handling state.
 *
 * Lock is acquired for most operations with this state.
 *
 * Global signal handling mechanics is implemented in the following way:
 *
 * - just before first signal's registration pipe is opened,
 *
 * - for each registered signal native handler is registered that just writes
 *   received signals into the pipe,
 *
 * - all registered signal services are asynchronously reading the pipe, and once
 *   some signal service reads signal number from the pipe, it notifies all
 *   registered services with this signal's handler.
 */
struct signal_service_state {
	signal_service_state();
	~signal_service_state();

	void add_server(base_server* server);
	void remove_server(base_server* server);

	// global mutex
	std::mutex lock;

	// separate thread for monitoring signal fd
	std::thread thread;
	boost::asio::io_service service;

	// handled signals
	sigset_t sigset;

	// signal fd
	int signal_descriptor;

	// registered servers
	std::set<base_server*> servers;

	// signal handlers
	signal_handler_type handlers[NSIG];
};

signal_service_state* get_signal_service_state();

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_SIGNAL_SERVICE_P_HPP
