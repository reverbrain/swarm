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

class event_listener
{
public:
	enum socket_action {
		socket_read = 0x01,
		socket_write = 0x02,
		socket_all = socket_read | socket_write
	};

	virtual ~event_listener();

	virtual void set_socket_data(int socket, void *data) = 0;
	virtual void on_timer() = 0;
	virtual void on_socket_event(int socket, int action) = 0;
};

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

	event_loop();
	virtual ~event_loop();

	void set_listener(event_listener *listener);
	event_listener *listener() const;

	void set_logger(const swarm::logger &logger);
	swarm::logger logger() const;

	virtual int open_socket(int domain, int type, int protocol);
	virtual int close_socket(int fd);
	virtual int socket_request(int socket, poll_option what, void *data) = 0;
	virtual int timer_request(long timeout_ms) = 0;
	virtual void post(const std::function<void ()> &func) = 0;

private:
	swarm::logger m_logger;
	event_listener *m_listener;
};

}} // namespace ioremap::swarm

#endif // IOREMAP_SWARM_EVENT_LOOP_H
