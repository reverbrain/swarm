#ifndef IOREMAP_THEVOID_SIGNAL_SERVICE_P_HPP
#define IOREMAP_THEVOID_SIGNAL_SERVICE_P_HPP

#include <signal.h>

#include <mutex>
#include <set>

#include <boost/asio.hpp>

#include <thevoid/server.hpp>

namespace ioremap { namespace thevoid {

/*!
 * \brief The signal_service class represents single server's registration.
 *
 * Signal handlers will be invoked for the server within passed io_service.
 *
 * On construction signal_service instance will be registered within global
 * signal handling mechanics and will be deregistered on destruction.
 */
struct signal_service {
	signal_service(boost::asio::io_service* service, base_server* server);
	~signal_service();

	boost::asio::io_service* service;
	base_server* server;
};

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

	// global mutex
	std::mutex lock;

	// read/write ends of pipe
	int read_descriptor;
	int write_descriptor;

	// registered signal services
	std::set<signal_service*> services;

	// flags that indicate registered signals
	bool registered[NSIG];

	// signal handlers
	signal_handler_type handlers[NSIG];
};

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_SIGNAL_SERVICE_P_HPP
