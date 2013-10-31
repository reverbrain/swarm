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

#include "ev_event_loop.hpp"

namespace ioremap {
namespace swarm {

ev_event_loop::ev_event_loop(ev::loop_ref &loop) :
	m_loop(loop), m_timer(loop), m_async(loop)
{
	m_timer.set<ev_event_loop, &ev_event_loop::on_timer>(this);
        m_async.set<ev_event_loop, &ev_event_loop::on_async>(this);
        m_async.start();
}

int ev_event_loop::socket_request(int socket, poll_option what, void *data)
{
	ev::io *io = reinterpret_cast<ev::io *>(data);

	if (what == poll_remove) {
		logger().log(LOG_DEBUG, "socket_callback, destroying io: %p, socket: %d, what: %d", io, socket, what);
		listener()->set_socket_data(socket, NULL);
		delete io;
		return 0;
	}

	if (!io) {
		io = new ev::io(m_loop);
		logger().log(LOG_DEBUG, "socket_callback, created io: %p, socket: %d, what: %d", io, socket, what);
		io->set<ev_event_loop, &ev_event_loop::on_socket_event>(this);

		listener()->set_socket_data(socket, io);
	}

	int events = 0;
	if (what & poll_in)
		events |= EV_READ;
	if (what & poll_out)
		events |= EV_WRITE;
	logger().log(LOG_DEBUG, "socket_callback, set io: %p, socket: %d, what: %d", io, socket, what);
	bool active = io->is_active();
	io->set(socket, events);
	if (!active)
		io->start();
	return 0;
}

int ev_event_loop::timer_request(long timeout_ms)
{
	m_timer.stop();
	m_timer.set(timeout_ms / 1000.);
	m_timer.start();
	return 0;
}

void ev_event_loop::post(const std::function<void ()> &func)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_events.push_back(func);
	}
	m_async.send();
}

void ev_event_loop::on_socket_event(ev::io &io, int revent)
{
        logger().log(LOG_DEBUG, "on_socket_event, io: %p, socket: %d, revent: %d", &io, io.fd, revent);


        int action = 0;
        if (revent & EV_READ)
	        action |= event_listener::socket_read;
        if (revent & EV_WRITE)
	        action |= event_listener::socket_write;

	listener()->on_socket_event(io.fd, action);
}

void ev_event_loop::on_timer(ev::timer &, int)
{
        logger().log(LOG_DEBUG, "on_timer");
	listener()->on_timer();
}

void ev_event_loop::on_async(ev::async &, int)
{
	logger().log(LOG_DEBUG, "on_async");

	for (;;) {
		std::function<void ()> event;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_events.empty())
				return;

			event = std::move(*m_events.begin());
			m_events.erase(m_events.begin());
		}

		if (event)
			event();
	}
}

} // namespace swarm
} // namespace ioremap
