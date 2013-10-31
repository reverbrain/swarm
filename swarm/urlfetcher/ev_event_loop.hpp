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

#ifndef IOREMAP_SWARM_EV_EVENT_LOOP_H
#define IOREMAP_SWARM_EV_EVENT_LOOP_H

#include "event_loop.hpp"

#include <mutex>
#include <list>

#define EV_MULTIPLICITY 1
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

#include <ev++.h>
#pragma GCC diagnostic pop

namespace ioremap {
namespace swarm {

class ev_event_loop : public event_loop
{
public:
	ev_event_loop(ev::loop_ref &loop);

	int socket_request(int socket, poll_option what, void *data);
	int timer_request(long timeout_ms);
	void post(const std::function<void ()> &func);

protected:
	void on_socket_event(ev::io &io, int revent);
	void on_timer(ev::timer &, int);
	void on_async(ev::async &, int);

private:
	ev::loop_ref &m_loop;
	ev::timer m_timer;
	ev::async m_async;
	std::mutex m_mutex;
	std::list<std::function<void ()>> m_events;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_EV_EVENT_LOOP_H
