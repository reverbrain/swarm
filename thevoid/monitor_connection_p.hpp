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

#ifndef IOREMAP_THEVOID_MONITOR_CONNECTION_P_HPP
#define IOREMAP_THEVOID_MONITOR_CONNECTION_P_HPP

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include "server.hpp"

namespace ioremap {
namespace thevoid {

class monitor_connection : public std::enable_shared_from_this<monitor_connection>
{
public:
	typedef boost::asio::ip::tcp::socket socket_type;
	typedef socket_type::endpoint_type endpoint_type;

	monitor_connection(base_server *server, boost::asio::io_service &io_service, size_t buffer_size);
	~monitor_connection();

	socket_type &socket();
	endpoint_type &endpoint();

	void start(const std::string &local_endpoint);

protected:
	std::string get_information();
	void async_read();
	void handle_read(const boost::system::error_code &err, std::size_t bytes_transferred);
	void async_write(const std::string &data);
	void handle_write(const boost::system::error_code &err, size_t);
	void handle_stop_write(const boost::system::error_code &err, size_t);
	void close();

private:
	base_server *m_server;
	socket_type m_socket;
	endpoint_type m_endpoint;
	boost::array<char, 64> m_buffer;
	std::string m_storage;
};

} // namespace thevoid
} // namespace ioremap

#endif // IOREMAP_THEVOID_MONITOR_CONNECTION_P_HPP
