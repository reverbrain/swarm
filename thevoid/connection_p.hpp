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

#ifndef IOREMAP_THEVOID_CONNECTION_P_HPP
#define IOREMAP_THEVOID_CONNECTION_P_HPP

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <swarm/http_request.hpp>
#include "request_parser_p.hpp"
#include "stream.hpp"
#include <queue>
#include <mutex>

#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ < 5
#  include <cstdatomic>
#  define SWARM_OLD_GCC
#else
#include <atomic>
#endif

namespace ioremap {
namespace thevoid {

class base_server;

struct buffer_info {
	buffer_info() : response(boost::none)
	{
	}

	template <typename A, typename B, typename C>
	buffer_info(A &&a, B &&b, C &&c) :
		buffer(std::move(a)),
		response(std::move(b)),
		handler(std::move(c))
	{
	}

	buffer_info(buffer_info &&info) :
		buffer(std::move(info.buffer)),
		response(std::move(info.response)),
		handler(std::move(info.handler))
	{
	}
	buffer_info(const buffer_info &info) = delete;

	buffer_info &operator =(buffer_info &&other)
	{
		buffer = std::move(other.buffer);
		response = std::move(other.response);
		handler = std::move(other.handler);

		return *this;
	}
	buffer_info &operator =(const buffer_info &info) = delete;

	std::vector<boost::asio::const_buffer> buffer;
	swarm::http_response response;
	std::function<void (const boost::system::error_code &err)> handler;
};

//! Represents a single connection from a client.
template <typename T>
class connection : public std::enable_shared_from_this<connection<T>>, public reply_stream, private boost::noncopyable
{
public:
	typedef T socket_type;
	typedef typename socket_type::endpoint_type endpoint_type;

	enum state {
		processing_request = 0x00,
		read_headers	   = 0x01,
		read_data		  = 0x02,
		request_processed  = 0x04,
		waiting_for_first_data = 0x08
	};

	//! Construct a connection with the given io_service.
	explicit connection(boost::asio::io_service &service, size_t buffer_size);
	~connection();

	//! Get the socket associated with the connection.
	socket_type &socket();
	endpoint_type &endpoint();

	//! Start the first asynchronous operation for the connection.
	void start(const std::shared_ptr<base_server> &server, const std::string &local_endpoint);

	virtual void send_headers(swarm::http_response &&rep,
		const boost::asio::const_buffer &content,
		std::function<void (const boost::system::error_code &err)> &&handler) /*override*/;
	virtual void send_data(const boost::asio::const_buffer &buffer,
		std::function<void (const boost::system::error_code &err)> &&handler) /*override*/;
	void want_more();
	virtual void close(const boost::system::error_code &err) /*override*/;

private:
	void want_more_impl();
	void send_impl(buffer_info &&info);
	void write_finished(const boost::system::error_code &err, size_t bytes_written);
	void send_nolock();

	void close_impl(const boost::system::error_code &err);
	void process_next();
	void print_access_log();

	//! Handle completion of a read operation.
	void handle_read(const boost::system::error_code &err, std::size_t bytes_transferred);
	void process_data(const char *begin, const char *end);

	void async_read();

	void send_error(swarm::http_response::status_type type);

	//! Server reference for handler logic
	std::shared_ptr<base_server> m_server;
	swarm::logger m_logger;

	//! Socket for the connection.
	socket_type m_socket;
	endpoint_type m_endpoint;

	//! Buffer for outgoing data
	std::deque<buffer_info> m_outgoing;
	std::mutex m_outgoing_mutex;

	//! Buffer for incoming data.
	std::vector<char> m_buffer;

	//! The incoming request.
	swarm::http_request m_request;

	//! Access log info
	std::string m_access_local;
	std::string m_access_remote;
	timeval m_access_start;
	std::string m_access_method;
	std::string m_access_url;
	int m_access_status;
	unsigned long long m_access_received;
	unsigned long long m_access_sent;

	//! The parser for the incoming request.
	request_parser m_request_parser;

	//! The estimated size of reply content-length which is not processed yet
	size_t m_content_length;

	//! This object represents the server logic
	std::shared_ptr<base_request_stream> m_handler;

	//! Request parsing state
	uint32_t m_state;
	bool m_sending;
	//! If current connection is keep-alive
	bool m_keep_alive;
	//! If async_read is already called
	bool m_at_read;

	//! Uprocessed data
	const char *m_unprocessed_begin;
	const char *m_unprocessed_end;
};

typedef connection<boost::asio::ip::tcp::socket> tcp_connection;
typedef connection<boost::asio::local::stream_protocol::socket> unix_connection;

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_CONNECTION_P_HPP
