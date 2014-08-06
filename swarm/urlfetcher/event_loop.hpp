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

#ifndef IOREMAP_SWARM_EVENT_LOOP_H
#define IOREMAP_SWARM_EVENT_LOOP_H

#include <functional>
#include "../logger.hpp"

namespace ioremap {
namespace swarm {

/*!
 * \brief The event_listener class is interface for receiving events from the event_loop.
 */
class event_listener
{
public:
	enum socket_action {
		socket_read = 0x01,
		socket_write = 0x02,
		socket_all = socket_read | socket_write
	};

	virtual ~event_listener();

	/*!
	 * \brief This method is called if event loop wants you to store
	 * some additional \a data specific for the \a socket.
	 */
	virtual void set_socket_data(int socket, void *data) = 0;
	/*!
	 * \brief This method is called once timer is elapsed.
	 */
	virtual void on_timer() = 0;
	/*!
	 * \brief This methods is called once \a socket has \a action available.
	 *
	 * \a Action is some of socket_action values.
	 */
	virtual void on_socket_event(int socket, int action) = 0;
};

/*!
 * \brief The event_loop class is an abstraction for event loop.
 *
 * It allows you to implement your own event loop for Swarm.
 */
class event_loop
{
public:
	enum poll_option {
		poll_none       = 0x00,
		poll_in         = 0x01,
		poll_out        = 0x02,
		poll_all        = poll_in | poll_out,
		poll_remove     = 0x04
	};

	event_loop(const swarm::logger &logger);
	virtual ~event_loop();

	/*!
	 * \brief Sets the \a listener of event loop's events.
	 *
	 * All events received by event loop should be passed to appropriate methods of \a listener.
	 */
	void set_listener(event_listener *listener);
	/*!
	 * \brief Returns previously listener of the event loop.
	 */
	event_listener *listener() const;

	/*!
	 * \brief Returns previously set logger.
	 */
	const swarm::logger &logger() const;

	/*!
	 * \brief Open socket for \a domain, \a type and \a protocol.
	 *
	 * Arguments are similiar to POSIX socket function.
	 *
	 * Default implementation just calls \a socket.
	 *
	 * On success, returnes socket descriptor. On error -1 is returned.
	 */
	virtual int open_socket(int domain, int type, int protocol);
	/*!
	 * \brief Close socket by \a fd.
	 *
	 * Default implemenation just calls \a close.
	 */
	virtual int close_socket(int fd);
	/*!
	 * \brief Asks event loop to poll \a what events for \a socket.
	 *
	 * Data previously set by listener::set_socket_data is provided by \a data.
	 *
	 * Returns 0 on success or error code otherwise.
	 */
	virtual int socket_request(int socket, poll_option what, void *data) = 0;
	/*!
	 * \brief Requests a timer call in \a timeout_ms milliseconds.
	 *
	 * Returns 0 on success or error code otherwise.
	 *
	 * \attention Only last set timer request will be in charge. All previously set timers are forgotten.
	 */
	virtual int timer_request(long timeout_ms) = 0;
	/*!
	 * \brief Invokes \a func in event loop thread.
	 *
	 * \attention It must be guaranteed that \a func doesn't throw exceptions otherwise behaviour is undefined.
	 */
	virtual void post(const std::function<void ()> &func) = 0;

private:
	swarm::logger m_logger;
	event_listener *m_listener;
};

}} // namespace ioremap::swarm

#endif // IOREMAP_SWARM_EVENT_LOOP_H
