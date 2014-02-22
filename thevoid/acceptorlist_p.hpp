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

#ifndef IOREMAP_THEVOID_ACCEPTORLIST_P_HPP
#define IOREMAP_THEVOID_ACCEPTORLIST_P_HPP

#include "server.hpp"
#include <boost/thread.hpp>

namespace ioremap {
namespace thevoid {

template <typename Connection>
class acceptors_list
{
public:
	typedef typename Connection::socket_type socket_type;
	typedef typename socket_type::protocol_type protocol_type;
	typedef typename protocol_type::endpoint endpoint_type;
	typedef boost::asio::basic_socket_acceptor<protocol_type> acceptor_type;
	typedef Connection connection_type;
	typedef std::shared_ptr<connection_type> connection_ptr_type;

	acceptors_list(server_data &data);
	~acceptors_list();

	void add_acceptor(const std::string &address);
	void start_acceptor(size_t index);
	void handle_accept(size_t index, connection_ptr_type conn, const boost::system::error_code &err);
    
    boost::asio::io_service &get_acceptor_service();
    boost::asio::io_service &get_connection_service();

	endpoint_type create_endpoint(acceptor_type &acc, const std::string &host);

	server_data &data;
	std::vector<std::unique_ptr<acceptor_type>> acceptors;
    std::vector<protocol_type> protocols;
};

} }

#endif // IOREMAP_THEVOID_ACCEPTORLIST_P_HPP
