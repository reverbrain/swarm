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

#include "monitor_connection_p.hpp"
#include "server_p.hpp"

#include <thevoid/rapidjson/stringbuffer.h>
#include <thevoid/rapidjson/prettywriter.h>

#include <boost/bind.hpp>

namespace ioremap {
namespace thevoid {

monitor_connection::monitor_connection(base_server *server, boost::asio::io_service &io_service, size_t buffer_size)
	: m_server(server), m_socket(io_service)
{
	(void) buffer_size;
}

monitor_connection::~monitor_connection()
{
}

monitor_connection::socket_type &monitor_connection::socket()
{
	return m_socket;
}

monitor_connection::endpoint_type &monitor_connection::endpoint()
{
	return m_endpoint;
}

void monitor_connection::start(const std::string &local_endpoint)
{
	(void) local_endpoint;
	async_read();
}

std::string monitor_connection::get_information()
{
	auto server_statistics = m_server->get_statistics();

	rapidjson::MemoryPoolAllocator<> allocator;
	rapidjson::Value information;
	information.SetObject();

	information.AddMember("connections", int(m_server->m_data->connections_counter), allocator);
	information.AddMember("active-connections", int(m_server->m_data->active_connections_counter), allocator);

	rapidjson::Value application;
	application.SetObject();

	for (auto it = server_statistics.begin(); it != server_statistics.end(); ++it) {
		application.AddMember(it->first.c_str(), it->second.c_str(), allocator);
	}

	information.AddMember("application", application, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

	information.Accept(writer);
	buffer.Put('\n');

	return std::string(buffer.GetString(), buffer.Size());
}

void monitor_connection::async_read()
{
	m_socket.async_read_some(boost::asio::buffer(m_buffer),
				 std::bind(&monitor_connection::handle_read, shared_from_this(),
					   std::placeholders::_1,
					   std::placeholders::_2));
}

void monitor_connection::handle_read(const boost::system::error_code &err, std::size_t bytes_transferred)
{
	if (err || bytes_transferred < 1) {
		close();
		return;
	}

	switch (m_buffer[0]) {
		case 'i': case 'I':
			async_write(get_information());
			break;
		case 's': case 'S': {
			const char *result = "Stopping...\n";
			boost::asio::async_write(m_socket, boost::asio::buffer(result, strlen(result)),
				std::bind(&monitor_connection::handle_stop_write, shared_from_this(),
					std::placeholders::_1, std::placeholders::_2));
			break;
		}
		default:
		case 'h': case 'H':
			async_write("i - statistics information\n"
				    "s - stop server\n"
				    "h - this help message\n");
			break;
	}
}

void monitor_connection::async_write(const std::string &data)
{
	m_storage = data;
	boost::asio::async_write(m_socket, boost::asio::buffer(m_storage),
				 std::bind(&monitor_connection::handle_write, shared_from_this(),
					   std::placeholders::_1,
					   std::placeholders::_2));
}

void monitor_connection::handle_write(const boost::system::error_code &, size_t)
{
	close();
}

void monitor_connection::handle_stop_write(const boost::system::error_code &, size_t)
{
	close();
	m_server->m_data->handle_stop();
}

void monitor_connection::close()
{
	// gratefull shutdown
	boost::system::error_code ignored_ec;
	m_socket.shutdown(boost::asio::socket_base::shutdown_both, ignored_ec);
}

} // namespace thevoid
} // namespace ioremap
