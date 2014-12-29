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

#include "boost_event_loop.hpp"

#include <blackhole/macro.hpp>
#include <boost/bind.hpp>
#include <boost/bind/placeholders.hpp>
#include <memory>

namespace ioremap {
namespace swarm {

boost_event_loop::boost_event_loop(boost::asio::io_service &service, const swarm::logger &logger) :
	event_loop(logger), m_service(service), m_timer(service)
{
}

struct boost_socket_info
{
	typedef std::shared_ptr<boost_socket_info> ptr;
	typedef std::weak_ptr<boost_socket_info> weak_ptr;

	boost_socket_info(boost::asio::io_service &service, int fd, event_loop::poll_option what) :
		socket(service), what(what)
	{
		socket.assign(boost::asio::local::stream_protocol(), dup(fd));
	}

	boost_socket_info(boost::asio::io_service &service, int fd) :
		socket(service), what(event_loop::poll_none)
	{
		socket.assign(boost::asio::local::stream_protocol(), fd);
	}

	template <typename... Args>
	static ptr *make_pointer(Args &&...args)
	{
		return new ptr(create(std::forward<Args>(args)...));
	}

	template <typename... Args>
	static ptr create(Args &&...args)
	{
		return std::make_shared<boost_socket_info>(std::forward<Args>(args)...);
	}

	boost::asio::local::stream_protocol::socket socket;
	event_loop::poll_option what;
};

int boost_event_loop::open_socket(int domain, int type, int protocol)
{
	int fd = event_loop::open_socket(domain, type, protocol);
	if (fd < 0) {
		int err = -errno;
		BH_LOG(logger(), SWARM_LOG_ERROR, "open_socket: failed, domain: %d, type: %d, protocol: %d, err: %d: %s",
			domain, type, protocol, err, strerror(-err));
		return -1;
	}

	auto socket = boost_socket_info::create(m_service, fd);

	BH_LOG(logger(), SWARM_LOG_DEBUG, "open_socket: %p, fd: %d, domain: %d, type: %d, protocol: %d",
		socket.get(), fd, domain, type, protocol);

	m_sockets.insert(std::make_pair(fd, socket));
	return fd;
}

int boost_event_loop::close_socket(int fd)
{
	auto it = m_sockets.find(fd);

	BH_LOG(logger(), SWARM_LOG_DEBUG, "close_socket: %p, fd: %d", it == m_sockets.end() ? it->second.get() : NULL, fd);

	if (it == m_sockets.end()) {
		return event_loop::close_socket(fd);
	}

	m_sockets.erase(it);
	return 0;
}

int boost_event_loop::socket_request(int fd, poll_option what, void *data)
{
//	std::cout << "socket_request, fd: " << fd << ", what: " << what << std::endl;
	boost_socket_info::ptr info;
	auto it = m_sockets.find(fd);

	if (it != m_sockets.end()) {
		info = it->second;

		if (what == poll_remove) {
			info->what = poll_none;
			info->socket.cancel();
		}
	} else {
		if (what == poll_remove) {
			if (data) {
				listener()->set_socket_data(fd, NULL);
				BH_LOG(logger(), SWARM_LOG_DEBUG, "remove socket: %p, fd: %d",
					(*reinterpret_cast<boost_socket_info::ptr *>(data)).get(), fd);
				delete reinterpret_cast<boost_socket_info::ptr *>(data);
			} else {
				// This is our own socket. It's already destroyed
			}
			return 0;
		}

		if (!data) {
			data = boost_socket_info::make_pointer(m_service, fd, what);
			BH_LOG(logger(), SWARM_LOG_DEBUG, "create socket: %p, fd: %d",
				(*reinterpret_cast<boost_socket_info::ptr *>(data)).get(), fd);
			listener()->set_socket_data(fd, data);
		}

		info = *reinterpret_cast<boost_socket_info::ptr *>(data);
	}

	BH_LOG(logger(), SWARM_LOG_DEBUG, "poll socket: %p, fd: %d, what: %d", info.get(), fd, what);
	info->what = what;

	boost_socket_info::weak_ptr weak_info = info;

	if (what & poll_in) {
		BH_LOG(logger(), SWARM_LOG_DEBUG, "poll in socket: %p, fd: %d", info.get(), fd);
		info->socket.async_read_some(boost::asio::null_buffers(),
			boost::bind(&boost_event_loop::on_event, this, fd, weak_info, event_listener::socket_read, _1));
	}
	if (what & poll_out) {
		BH_LOG(logger(), SWARM_LOG_DEBUG, "poll out socket: %p, fd: %d", info.get(), fd);
		info->socket.async_write_some(boost::asio::null_buffers(),
			boost::bind(&boost_event_loop::on_event, this, fd, weak_info, event_listener::socket_write, _1));
	}

	return 0;
}

int boost_event_loop::timer_request(long timeout_ms)
{
	BH_LOG(logger(), SWARM_LOG_DEBUG, "timer: %ld", timeout_ms);
	m_timer.cancel();

	if (timeout_ms == 0) {
		m_service.post(std::bind(&event_listener::on_timer, listener()));
	} else if (timeout_ms > 0) {
		m_timer.expires_from_now(boost::posix_time::millisec(timeout_ms));
		m_timer.async_wait(std::bind(&event_listener::on_timer, listener()));
	}

	return 0;
}

void boost_event_loop::post(const std::function<void ()> &func)
{
	BH_LOG(logger(), SWARM_LOG_DEBUG, "post");
	m_service.dispatch(func);
}

void boost_event_loop::on_event(int fd, const boost_socket_info::weak_ptr &weak_info, int what, const boost::system::error_code &error)
{
	if (auto info = weak_info.lock()) {
		BH_LOG(logger(), SWARM_LOG_DEBUG, "on_event socket: %p, fd: %d, info->what: %d, what: %d, error: %s",
				 info.get(), fd, info->what, what, error.message().c_str());

		if (what == event_listener::socket_read && (info->what & poll_in)) {
			BH_LOG(logger(), SWARM_LOG_DEBUG, "repoll in socket: %p, fd: %d", info.get(), fd);
			info->socket.async_read_some(boost::asio::null_buffers(),
				boost::bind(&boost_event_loop::on_event, this, fd, weak_info, event_listener::socket_read, _1));
		}
		if (what == event_listener::socket_write && (info->what & poll_out)) {
			BH_LOG(logger(), SWARM_LOG_DEBUG, "repoll out socket: %p, fd: %d", info.get(), fd);
			info->socket.async_write_some(boost::asio::null_buffers(),
				boost::bind(&boost_event_loop::on_event, this, fd, weak_info, event_listener::socket_write, _1));
		}

		BH_LOG(logger(), SWARM_LOG_DEBUG, "call on_socket_event: %p, fd: %d", info.get(), fd);

		if (info->what != poll_none) {
			listener()->on_socket_event(fd, what);
		}
	} else {
		BH_LOG(logger(), SWARM_LOG_DEBUG, "call on_socket_event: socket_info is destroyed, fd: %d, what: %d, error: %s",
				 fd, what, error.message().c_str());
	}
}

} // namespace swarm
} // namespace ioremap
