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

#include "connection_p.hpp"
#include <vector>
#include <boost/bind.hpp>
#include <iostream>

#include "server_p.hpp"
#include "stockreplies_p.hpp"

namespace ioremap {
namespace thevoid {

//#define debug(arg) do { std::cerr << __PRETTY_FUNCTION__ << " (" << __LINE__ << ") " << arg << std::endl; std::cerr.flush(); } while (0)

#define debug(arg) do {} while (0)

#define SAFE_SEND_NONE do {} while (0)
#define SAFE_SEND_ERROR \
do { \
	if (!m_something_sent) { \
		send_error(swarm::http_response::internal_server_error); \
	} else { \
		boost::system::error_code ignored_ec; \
		m_socket.shutdown(boost::asio::socket_base::shutdown_both, ignored_ec); \
	} \
	return; \
} while (0)

#define SAFE_CALL(expr, err_prefix, error_handler) \
do { \
	if (m_server->m_data->safe_mode) { \
		try { \
			expr; \
		} catch (const std::exception &ex) { \
			m_server->logger().log(swarm::SWARM_LOG_ERROR, "%s: uncaught exception: %s", (err_prefix), ex.what()); \
			error_handler; \
		} catch (...) { \
			m_server->logger().log(swarm::SWARM_LOG_ERROR, "%s: uncaught exception: unknown", (err_prefix)); \
			error_handler; \
		} \
	} else { \
		expr; \
	} \
} while (0)

template <typename T>
connection<T>::connection(boost::asio::io_service &service, size_t buffer_size) :
	m_socket(service),
	m_sending(false),
	m_something_sent(false),
	m_buffer(buffer_size),
	m_content_length(0),
	m_state(read_headers),
	m_keep_alive(false),
	m_at_read(false)
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
		SAFE_CALL(m_handler->on_close(boost::system::error_code()), "connection::~connection", SAFE_SEND_NONE);

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
void connection<T>::send_headers(swarm::http_response &&rep,
	const boost::asio::const_buffer &content,
	std::function<void (const boost::system::error_code &err)> &&handler)
{
	if (m_keep_alive) {
                rep.headers().set_keep_alive();
                debug("Added Keep-Alive");
        }

	buffer_info info(
		std::move(stock_replies::to_buffers(rep, content)),
		std::move(rep),
		std::move(handler)
	);
	send_impl(std::move(info));
}

template <typename T>
void connection<T>::send_data(const boost::asio::const_buffer &buffer,
	std::function<void (const boost::system::error_code &)> &&handler)
{
	buffer_info info(
		std::move(std::vector<boost::asio::const_buffer>(1, buffer)),
		boost::none,
		std::move(handler)
	);
	send_impl(std::move(info));
}

template <typename T>
void connection<T>::want_more()
{
	// Invoke close_impl some time later, so we won't need any mutexes to guard the logic
	m_socket.get_io_service().post(std::bind(&connection::want_more_impl, this->shared_from_this()));
}

template <typename T>
void connection<T>::close(const boost::system::error_code &err)
{
	// Invoke close_impl some time later, so we won't need any mutexes to guard the logic
	m_socket.get_io_service().dispatch(std::bind(&connection::close_impl, this->shared_from_this(), err));
}

template <typename T>
void connection<T>::want_more_impl()
{
	if (m_unprocessed_begin != m_unprocessed_end) {
		process_data(m_unprocessed_begin, m_unprocessed_end);
	} else {
		async_read();
	}
}

template <typename T>
void connection<T>::send_impl(buffer_info &&info)
{
	std::lock_guard<std::mutex> lock(m_outgoing_mutex);

	m_outgoing.emplace_back(std::move(info));

	if (!m_sending) {
		m_sending = true;
		send_nolock();
	}

	m_something_sent = true;
}

template <typename T>
void connection<T>::write_finished(const boost::system::error_code &err, size_t bytes_written)
{
	if (err) {
		decltype(m_outgoing) outgoing;
		{
			std::lock_guard<std::mutex> lock(m_outgoing_mutex);
			outgoing = std::move(m_outgoing);
		}

		for (auto it = outgoing.begin(); it != outgoing.end(); ++it) {
			if (it->handler)
				it->handler(err);
		}

		if (m_handler)
			SAFE_CALL(m_handler->on_close(err), "connection::write_finished", SAFE_SEND_NONE);
		close_impl(err);
		return;
	}

	while (bytes_written) {
		std::unique_lock<std::mutex> lock(m_outgoing_mutex);
		if (m_outgoing.empty()) {
			m_server->logger().log(swarm::SWARM_LOG_ERROR, "connection::write_finished: extra written bytes: %zu", bytes_written);
			break;
		}

		auto &buffers = m_outgoing.front().buffer;

		auto it = buffers.begin();

		size_t buffer_size = 0;
		for (auto jt = buffers.begin(); jt != buffers.end(); ++jt) {
			buffer_size += boost::asio::buffer_size(*jt);
		}

		for (; it != buffers.end(); ++it) {
			const size_t size = boost::asio::buffer_size(*it);
			if (size <= bytes_written) {
				bytes_written -= size;
			} else {
				*it = bytes_written + *it;
				bytes_written = 0;
				break;
			}
		}

		if (it == buffers.end()) {
			const auto handler = std::move(m_outgoing.front().handler);
			m_outgoing.pop_front();
			if (handler) {
				lock.unlock();
				handler(err);
				lock.lock();
			}
		} else {
			buffers.erase(buffers.begin(), it);
		}
	}

	std::unique_lock<std::mutex> lock(m_outgoing_mutex);
	if (m_outgoing.empty()) {
		m_sending = false;
		return;
	}

	send_nolock();
}

class buffers_array
{
private:
	enum {
		buffers_count = 32
	};
public:
	typedef boost::asio::const_buffer value_type;
	typedef const value_type * const_iterator;

	template <typename Iterator>
	buffers_array(Iterator begin, Iterator end) :
		m_size(0)
	{
		for (auto it = begin; it != end && m_size < buffers_count; ++it) {
			for (auto jt = it->buffer.begin(); jt != it->buffer.end() && m_size < buffers_count; ++jt) {
				m_data[m_size++] = *jt;
			}
		}
	}

	const_iterator begin() const
	{
		return m_data;
	}

	const_iterator end() const
	{
		return &m_data[m_size];
	}

private:
	value_type m_data[buffers_count];
	size_t m_size;
};

template <typename T>
void connection<T>::send_nolock()
{
	buffers_array data(m_outgoing.begin(), m_outgoing.end());

	m_socket.async_write_some(data, std::bind(
		&connection::write_finished, this->shared_from_this(),
		std::placeholders::_1, std::placeholders::_2));
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

	if (!m_keep_alive) {
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
	m_at_read = false;
	debug("error: " << err.message());
	if (err) {
		if (m_handler) {
			SAFE_CALL(m_handler->on_close(err), "connection::handle_read", SAFE_SEND_NONE);
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

			m_keep_alive = false;
			send_error(swarm::http_response::bad_request);
			return;
		} else if (result) {
//			std::cerr << "url: " << m_request.uri << std::endl;

			auto factory = m_server->factory(m_request);

			if (auto length = m_request.headers().content_length())
				m_content_length = *length;
			else
				m_content_length = 0;
			m_keep_alive = m_request.is_keep_alive();

			if (factory) {
				++m_server->m_data->active_connections_counter;
				m_handler = factory->create();
				m_handler->initialize(std::static_pointer_cast<reply_stream>(this->shared_from_this()));
				SAFE_CALL(m_handler->on_headers(std::move(m_request)), "connection::process_data", SAFE_SEND_ERROR);
			} else {
				send_error(swarm::http_response::not_found);
			}

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
		size_t processed_size = data_from_body;

		if (data_from_body && m_handler)
			SAFE_CALL(processed_size = m_handler->on_data(boost::asio::buffer(begin, data_from_body)), "connection::process_data", SAFE_SEND_ERROR);

		m_content_length -= processed_size;

		debug(m_state);

		if (data_from_body != processed_size) {
			// Handler can't process all data, wait until want_more method is called
			m_unprocessed_begin = begin + processed_size;
			m_unprocessed_end = end;
			return;
		} else if (m_content_length > 0) {
			debug("");
			async_read();
		} else {
			m_state &= ~read_data;
			m_unprocessed_begin = begin + processed_size;
			m_unprocessed_end = end;

			if (m_handler)
				SAFE_CALL(m_handler->on_close(boost::system::error_code()), "connection::process_data", SAFE_SEND_ERROR);

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
	if (m_at_read)
		return;
	m_at_read = true;
	m_unprocessed_begin = NULL;
	m_unprocessed_end = NULL;
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
