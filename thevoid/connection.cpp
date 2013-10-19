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

#include "connection_p.hpp"
#include <vector>
#include <boost/bind.hpp>
#include <iostream>

#if __GNUC__ == 4 && __GNUC_MINOR__ < 5
#  include <cstdatomic>
#else
#  include <atomic>
#endif

#include "server_p.hpp"
#include "stockreplies_p.hpp"

namespace ioremap {
namespace thevoid {

//#define debug(arg) do { std::cerr << __PRETTY_FUNCTION__ << " (" << __LINE__ << ") " << arg << std::endl; std::cerr.flush(); } while (0)

#define debug(arg) do {} while (0)

template <typename T>
connection<T>::connection(boost::asio::io_service &service, size_t buffer_size) :
	m_socket(service),
	m_buffer(buffer_size),
	m_content_length(0),
	m_state(read_headers)
{
	m_unprocessed_begin = m_buffer.data();
	m_unprocessed_end = m_buffer.data();

	debug(&service);
}

template <typename T>
connection<T>::~connection()
{
	if (m_server)
		--m_server->m_data->connections_counter;

	if (m_handler)
		m_handler->on_close(boost::system::error_code());

	debug("");
}

template <typename T>
T &connection<T>::socket()
{
	return m_socket;
}

template <typename T>
void connection<T>::start(const std::shared_ptr<base_server> &server)
{
	m_server = server;
	++m_server->m_data->connections_counter;
	async_read();
}

struct send_headers_guard
{
	std::function<void (const boost::system::error_code &)> handler;
	std::shared_ptr<swarm::http_response> reply;

	template <typename T>
	void operator() (const boost::system::error_code &err, const T &)
	{
		handler(err);
	}
};

template <typename T>
void connection<T>::send_headers(const swarm::http_response &rep,
	const boost::asio::const_buffer &content,
	const std::function<void (const boost::system::error_code &err)> &handler)
{
	send_headers_guard guard = {
		handler,
		std::make_shared<swarm::http_response>(rep)
	};

	if (m_request.is_keep_alive()) {
		guard.reply->set_header("Connection", "Keep-Alive");
		debug("Added Keep-Alive");
	}

	boost::asio::async_write(m_socket, stock_replies::to_buffers(*guard.reply, content), guard);
}

template <typename T>
void connection<T>::send_data(const boost::asio::const_buffer &buffer,
	const std::function<void (const boost::system::error_code &)> &handler)
{
	boost::asio::async_write(m_socket, boost::asio::buffer(buffer), boost::bind(handler, _1));
}

template <typename T>
void connection<T>::close(const boost::system::error_code &err)
{
	// Invoke close_impl some time later, so we won't need any mutexes to guard the logic
	m_socket.get_io_service().post(std::bind(&connection::close_impl, this->shared_from_this(), err));
}

template <typename T>
void connection<T>::close_impl(const boost::system::error_code &err)
{
	debug(m_state);
	if (m_handler)
		--m_server->m_data->active_connections_counter;
	m_handler.reset();

	if (err) {
		boost::system::error_code ignored_ec;
		// If there was any error - close the connection, it's broken
		m_socket.shutdown(boost::asio::socket_base::shutdown_both, ignored_ec);
		return;
	}

	// Is request data is not fully received yet - receive it
	if (m_state != processing_request) {
		m_state |= request_processed;
		return;
	}

	if (!m_request.is_keep_alive()) {
		boost::system::error_code ignored_ec;
		m_socket.shutdown(boost::asio::socket_base::shutdown_both, ignored_ec);
		return;
	}

	process_next();
}

template <typename T>
void connection<T>::process_next()
{
	// Start to wait new HTTP requests by this socket due to HTTP 1.1
	m_state = read_headers;
	m_request_parser.reset();

	m_request = swarm::http_request();

	if (m_unprocessed_begin != m_unprocessed_end) {
		process_data(m_unprocessed_begin, m_unprocessed_end);
	} else {
		async_read();
	}
}

template <typename T>
void connection<T>::handle_read(const boost::system::error_code &err, std::size_t bytes_transferred)
{
	debug("error: " << err.message());
	if (err) {
		if (m_handler) {
			m_handler->on_close(err);
			--m_server->m_data->active_connections_counter;
		}
		m_handler.reset();
		return;
	}

	process_data(m_buffer.data(), m_buffer.data() + bytes_transferred);

	// If an error occurs then no new asynchronous operations are started. This
	// means that all shared_ptr references to the connection object will
	// disappear and the object will be destroyed automatically after this
	// handler returns. The connection class's destructor closes the socket.
}

template <typename T>
void connection<T>::process_data(const char *begin, const char *end)
{
	debug("data: \"" << std::string(begin, end - begin) << "\"");
	if (m_state & read_headers) {
		boost::tribool result;
		const char *new_begin = NULL;
		boost::tie(result, new_begin) = m_request_parser.parse(m_request, begin, end);

		debug("parsed: \"" << std::string(begin, new_begin) << '"');
		debug("parse result: " << (result ? "true" : (!result ? "false" : "unknown_state")));

		if (!result) {
//			std::cerr << "url: " << m_request.uri << std::endl;

			send_error(swarm::http_response::bad_request);
			return;
		} else if (result) {
//			std::cerr << "url: " << m_request.uri << std::endl;

			auto factory = m_server->get_factory(m_request.url());
			if (!factory) {
				send_error(swarm::http_response::not_found);
				return;
			}

			m_content_length = m_request.get_content_length();

			++m_server->m_data->active_connections_counter;
			m_handler = factory->create();
			m_handler->initialize(std::static_pointer_cast<reply_stream>(this->shared_from_this()));
			m_handler->on_headers(m_request);

			m_state &= ~read_headers;
			m_state |=  read_data;

			process_data(new_begin, end);
			// async_read is called by processed_data
			return;
		}

		// need more data for request processing
		async_read();
	} else if (m_state & read_data) {
		size_t data_from_body = std::min<size_t>(m_content_length, end - begin);

		if (data_from_body && m_handler)
			m_handler->on_data(boost::asio::buffer(begin, data_from_body));

		m_content_length -= data_from_body;

		debug(m_state);

		if (m_content_length > 0) {
			debug("");
			async_read();
		} else {
			m_state &= ~read_data;
			m_unprocessed_begin = begin + data_from_body;
			m_unprocessed_end = end;

			if (m_handler)
				m_handler->on_close(boost::system::error_code());

			if (m_state & request_processed) {
				debug("");
				process_next();
			}
		}
	}
}

template <typename T>
void connection<T>::async_read()
{
	debug("");
	m_socket.async_read_some(boost::asio::buffer(m_buffer),
					 std::bind(&connection::handle_read, this->shared_from_this(),
						   std::placeholders::_1,
						   std::placeholders::_2));
}

template <typename T>
void connection<T>::send_error(swarm::http_response::status_type type)
{
	send_headers(stock_replies::stock_reply(type),
		boost::asio::const_buffer(),
		std::bind(&connection::close_impl, this->shared_from_this(), std::placeholders::_1));
}

template class connection<boost::asio::local::stream_protocol::socket>;
template class connection<boost::asio::ip::tcp::socket>;

} // namespace thevoid
} // namespace ioremap
