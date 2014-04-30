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

#ifndef IOREMAP_SWARM_BOOST_EVENT_LOOP_H
#define IOREMAP_SWARM_BOOST_EVENT_LOOP_H

#include "event_loop.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <unordered_map>

namespace ioremap {
namespace swarm {

struct boost_socket_info;

/*!
 * \brief The boost_event_loop is boost::asio-based event loop.
 */
class boost_event_loop : public event_loop
{
public:
	boost_event_loop(boost::asio::io_service &service);

	int open_socket(int domain, int type, int protocol);
	int close_socket(int fd);
	int socket_request(int socket, poll_option what, void *data);
	int timer_request(long timeout_ms);
	void post(const std::function<void ()> &func);

private:
	void on_event(int fd, const std::weak_ptr<boost_socket_info> &info, int what, const boost::system::error_code &error);

	boost::asio::io_service &m_service;
	boost::asio::deadline_timer m_timer;
	std::unordered_map<int, std::shared_ptr<boost_socket_info>> m_sockets;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_BOOST_EVENT_LOOP_H
