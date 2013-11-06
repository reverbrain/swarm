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

#include "acceptorlist_p.hpp"
#include "server_p.hpp"
#include "monitor_connection_p.hpp"
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <sys/stat.h>

namespace ioremap { namespace thevoid {

enum { MAX_CONNECTIONS_COUNT = 128 };

template <typename Endpoint>
static void complete_socket_creation(Endpoint endpoint)
{
	(void) endpoint;
}

static void complete_socket_creation(boost::asio::local::stream_protocol::endpoint endpoint)
{
	chmod(endpoint.path().c_str(), 0666);
}

template <typename Connection>
void acceptors_list<Connection>::add_acceptor(const std::string &address)
{
	acceptors.emplace_back(new acceptor_type(get_acceptor_service()));

	auto &acceptor = acceptors.back();

	try {
		endpoint_type endpoint = create_endpoint(*acceptor, address);

		acceptor->open(endpoint.protocol());
		acceptor->set_option(boost::asio::socket_base::reuse_address(true));
		acceptor->bind(endpoint);
		acceptor->listen(data.backlog_size);

		protocols.push_back(endpoint.protocol());

		complete_socket_creation(endpoint);
	} catch (boost::system::system_error &error) {
		std::cerr << "Can not bind socket \"" << address << "\": " << error.what() << std::endl;
		std::cerr.flush();
		throw;
	}

	start_acceptor(acceptors.size() - 1);
}

template <typename Connection>
void acceptors_list<Connection>::start_acceptor(size_t index)
{
	acceptor_type &acc = *acceptors[index];

	auto conn = std::make_shared<connection_type>(get_connection_service(), data.buffer_size);

	acc.async_accept(conn->socket(), boost::bind(
				 &acceptors_list::handle_accept, this, index, conn, _1));
}

template <typename Connection>
void acceptors_list<Connection>::handle_accept(size_t index, connection_ptr_type conn, const boost::system::error_code &err)
{
	if (!err) {
		if (auto server = data.server.lock()) {
			conn->start(server);
		} else {
			throw std::logic_error("server::m_data->server is null");
		}
	} else {
		data.logger.log(swarm::SWARM_LOG_ERROR, "Failed to accept connection: %s", err.message().c_str());
	}

	start_acceptor(index);
}

template <typename Connection>
boost::asio::io_service &acceptors_list<Connection>::get_acceptor_service()
{
	return data.io_service;
}

template <typename Connection>
boost::asio::io_service &acceptors_list<Connection>::get_connection_service()
{
	return data.get_worker_service();
}

template <>
boost::asio::io_service &acceptors_list<monitor_connection>::get_acceptor_service()
{
	return data.monitor_io_service;
}

template <>
boost::asio::io_service &acceptors_list<monitor_connection>::get_connection_service()
{
	return data.monitor_io_service;
}

template <typename Connection>
typename acceptors_list<Connection>::endpoint_type acceptors_list<Connection>::create_endpoint(acceptor_type &acc, const std::string &host)
{
	(void) acc;

	size_t delim = host.find_last_of(':');
	auto address = boost::asio::ip::address::from_string(host.substr(0, delim));
	auto port = boost::lexical_cast<unsigned short>(host.substr(delim + 1));

	boost::asio::ip::tcp::endpoint endpoint(address, port);

	return endpoint;
}

template <>
acceptors_list<unix_connection>::~acceptors_list()
{
	for (size_t i = 0; i < acceptors.size(); ++i) {
		auto &acceptor = *acceptors[i];
		auto path = acceptor.local_endpoint().path();
		unlink(path.c_str());
	}
}

template <>
acceptors_list<unix_connection>::endpoint_type acceptors_list<unix_connection>::create_endpoint(acceptor_type &acc,
		const std::string &host)
{
	(void) acc;
	return boost::asio::local::stream_protocol::endpoint(host);
}

template class acceptors_list<tcp_connection>;
template class acceptors_list<unix_connection>;
template class acceptors_list<monitor_connection>;

} }
