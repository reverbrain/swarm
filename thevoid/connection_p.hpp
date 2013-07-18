/*
 * 2013+ Copyright (c) Ruslan Nigatullin <euroelessar@yandex.ru>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef IOREMAP_THEVOID_CONNECTION_P_HPP
#define IOREMAP_THEVOID_CONNECTION_P_HPP

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <swarm/networkrequest.h>
#include "request_parser_p.hpp"
#include "stream.hpp"

namespace ioremap {
namespace thevoid {

class base_server;

//! Represents a single connection from a client.
template <typename T>
class connection : public std::enable_shared_from_this<connection<T>>, public reply_stream, private boost::noncopyable
{
public:
    typedef T socket_type;

	enum state {
		processing_request = 0x00,
		read_headers       = 0x01,
		read_data          = 0x02,
		request_processed  = 0x04
	};

	//! Construct a connection with the given io_service.
	explicit connection(boost::asio::io_service &service);
	~connection();

	//! Get the socket associated with the connection.
	T &socket();

	//! Start the first asynchronous operation for the connection.
	void start(const std::shared_ptr<base_server> &server);

	virtual void send_headers(const swarm::network_reply &rep,
		const boost::asio::const_buffer &content,
		const std::function<void (const boost::system::error_code &err)> &handler) /*override*/;
	virtual void send_data(const boost::asio::const_buffer &buffer,
		const std::function<void (const boost::system::error_code &err)> &handler) /*override*/;
	virtual void close(const boost::system::error_code &err) /*override*/;

private:
    void send_headers_impl(const swarm::network_reply &rep,
                           const boost::asio::const_buffer &content,
                           const std::function<void (const boost::system::error_code &err)> &handler);
    void close_impl(const boost::system::error_code &err);
    void process_next();

	//! Handle completion of a read operation.
	void handle_read(const boost::system::error_code &err, std::size_t bytes_transferred);
	void process_data(char *begin, char *end);

	void async_read();

	void send_error(swarm::network_reply::status_type type);

	//! Handle completion of a write operation.
	void handle_write(const boost::system::error_code &e);

	//! Server reference for handler logic
	std::shared_ptr<base_server> m_server;

	//! Strand to ensure the connection's handlers are not called concurrently.
	boost::asio::io_service::strand m_strand;

	//! Socket for the connection.
	T m_socket;

	//! Buffer for incoming data.
	boost::array<char, 8192> m_buffer;

	//! The incoming request.
	swarm::network_request m_request;

	//! The parser for the incoming request.
	request_parser m_request_parser;

	//! The reply to be sent back to the client.
	swarm::network_reply m_reply;

	//! The estimated size of reply content-length which is not processed yet
	size_t m_content_length;

	//! This object represents the server logic
	std::shared_ptr<base_request_stream> m_handler;

	//! Request parsing state
	uint32_t m_state;

	//! Uprocessed data
	char *m_unprocessed_begin;
	char *m_unprocessed_end;
};

typedef connection<boost::asio::ip::tcp::socket> tcp_connection;
typedef connection<boost::asio::local::stream_protocol::socket> unix_connection;

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_CONNECTION_P_HPP
