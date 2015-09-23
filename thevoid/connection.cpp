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
#include <algorithm>

#include "server_p.hpp"
#include "stream_p.hpp"

namespace {

const long int USECS_IN_SEC = 1000 * 1000;
const long int NSECS_IN_SEC = USECS_IN_SEC * 1000;

struct timespec& operator+= (struct timespec& lhs, const struct timespec& rhs) {
	lhs.tv_sec += rhs.tv_sec;
	lhs.tv_nsec += rhs.tv_nsec;

	if (lhs.tv_nsec >= NSECS_IN_SEC) {
		lhs.tv_sec += 1;
		lhs.tv_nsec -= NSECS_IN_SEC;
	}

	return lhs;
}

struct timespec operator+ (struct timespec lhs, const struct timespec& rhs) {
	return lhs += rhs;
}

struct timespec& operator-= (struct timespec& lhs, const struct timespec& rhs) {
	lhs.tv_sec -= rhs.tv_sec;
	lhs.tv_nsec -= rhs.tv_nsec;

	if (lhs.tv_nsec < 0) {
		lhs.tv_sec -= 1;
		lhs.tv_nsec += NSECS_IN_SEC;
	}

	if (lhs.tv_sec < 0) {
		lhs = {0, 0};
	}

	return lhs;
}

struct timespec operator- (struct timespec lhs, const struct timespec& rhs) {
	return lhs -= rhs;
}

long int timespec_to_usec(const struct timespec& t) {
	return t.tv_sec * USECS_IN_SEC + t.tv_nsec / 1000;
}

struct timespec gettime_now() {
	struct timespec now;

#ifdef CLOCK_MONOTONIC_RAW
	if (clock_gettime(CLOCK_MONOTONIC_RAW, &now) == 0) {
		return now;
	}
#endif

	clock_gettime(CLOCK_MONOTONIC, &now);
	return now;
}

} // unnamed namespace


namespace {

template <typename InputIterator, typename OutputIterator>
OutputIterator escape(
		InputIterator begin, InputIterator end,
		OutputIterator output,
		int quote
	)
{
	for (auto iter = begin; iter != end; ++iter) {
		char ch = *iter;

		if (ch == '\a') {
			output++ = '\\'; output++ = 'a';
		}
		else if (ch == '\b') {
			output++ = '\\'; output++ = 'b';
		}
		else if (ch == '\f') {
			output++ = '\\'; output++ = 'f';
		}
		else if (ch == '\n') {
			output++ = '\\'; output++ = 'n';
		}
		else if (ch == '\r') {
			output++ = '\\'; output++ = 'r';
		}
		else if (ch == '\t') {
			output++ = '\\'; output++ = 't';
		}
		else if (ch == '\v') {
			output++ = '\\'; output++ = 'v';
		}
		else if (ch == quote || ch == '\\') {
			output++ = '\\'; output++ = ch;
		}
		else if (ch < ' ' || ch >= 0x7f) {
			char buf[3];
			sprintf(buf, "%02x", ch & 0xff);
			output++ = '\\'; output++ = 'x'; output++ = buf[0]; output++ = buf[1];
		}
		else {
			output++ = ch;
		}
	}

	return output;
}

std::string headers_to_string(
		const ioremap::swarm::http_headers& headers,
		const std::vector<std::string>& log_headers,
		int quote = '"'
	)
{
	typedef std::back_insert_iterator<std::string> output_type;

	std::string headers_string;
	output_type output(headers_string);

	output++ = '{';

	bool first_header = true;
	for (const auto& log_header: log_headers) {
		if (headers.has(log_header)) {
			const auto& header_value = *headers.get(log_header);

			if (!first_header) {
				output++ = ','; output++ = ' ';
			}
			else {
				first_header = false;
			}

			output++ = quote;
			output = std::copy(log_header.begin(), log_header.end(), output);
			output++ = ':'; output++ = ' ';
			output = escape(header_value.begin(), header_value.end(), output, quote);
			output++ = quote;
		}
	}

	output++ = '}';

	return headers_string;
}

} // unnamed namespace


namespace ioremap {
namespace thevoid {

#define CONNECTION_LOG(log_level, ...) \
	BH_LOG(m_logger, (log_level), __VA_ARGS__)

#define CONNECTION_DEBUG(...) \
	CONNECTION_LOG(SWARM_LOG_DEBUG, __VA_ARGS__)

#define CONNECTION_INFO(...) \
	CONNECTION_LOG(SWARM_LOG_INFO, __VA_ARGS__)

#define CONNECTION_ERROR(...) \
	CONNECTION_LOG(SWARM_LOG_ERROR, __VA_ARGS__)

#define SAFE_SEND_NONE do {} while (0)
#define SAFE_SEND_ERROR \
do { \
	boost::system::error_code ignored_ec; \
	m_socket.shutdown(boost::asio::socket_base::shutdown_both, ignored_ec); \
	m_socket.close(ignored_ec); \
	if (m_handler) { \
		--m_server->m_data->active_connections_counter; \
		m_handler.reset(); \
	} \
	return; \
} while (0)

#define SAFE_CALL(expr, err_prefix, error_handler) \
do { \
	if (m_server->m_data->safe_mode) { \
		try { \
			expr; \
		} catch (const std::exception &ex) { \
			CONNECTION_ERROR("uncaught exception") \
				("context", (err_prefix)) \
				("error", ex.what()); \
			m_access_status = 598; \
			print_access_log(); \
			error_handler; \
		} catch (...) { \
			CONNECTION_ERROR("uncaught exception") \
				("context", (err_prefix)) \
				("error", "unknown"); \
			m_access_status = 598; \
			print_access_log(); \
			error_handler; \
		} \
	} else { \
		expr; \
	} \
} while (0)

static blackhole::log::attributes_t make_attributes(void *connection)
{
	char buffer[128];
	snprintf(buffer, sizeof(buffer), "%p", connection);

	blackhole::log::attributes_t attributes = {
		blackhole::attribute::make(std::string("connection"), std::string(buffer))
	};
	return std::move(attributes);
}

template <typename T>
connection<T>::connection(base_server *server, boost::asio::io_service &service, size_t buffer_size) :
	m_server(server),
	m_base_logger(m_server->logger(), make_attributes(this)),
	m_logger(m_base_logger, blackhole::log::attributes_t()),
	m_socket(service),
	m_buffer(buffer_size),
	m_content_length(0),
	m_access_log_printed(false),
	m_close_invoked(false),
	m_state(read_headers | waiting_for_first_data),
	m_sending(false),
	m_keep_alive(false),
	m_at_read(false),
	m_pause_receive(false),
	m_receive_time{0, 0},
	m_send_time{0, 0},
	m_starttransfer_time{0, 0}
{
	m_unprocessed_begin = m_buffer.data();
	m_unprocessed_end = m_buffer.data();
	m_access_start.tv_sec = 0;
	m_access_start.tv_usec = 0;
	m_access_status = 0;
	m_access_received = 0;
	m_access_sent = 0;
	m_request_processing_was_finished = false;

	CONNECTION_DEBUG("connection created")
		("service", &service);
}

template <typename T>
connection<T>::~connection()
{
	if (m_server) {
		CONNECTION_INFO("connection to client closed");
		--m_server->m_data->connections_counter;
	}

	if (m_handler) {
		m_access_status = 597;
		print_access_log();
	}

	// This isn't actually possible.
	// Handler keeps pointer to the connection, if the connection has pointer to the handler
	// they prolong lifetime of each other.
	/*
	if (auto handler = try_handler())
		SAFE_CALL(handler->on_close(boost::system::error_code()), "connection::~connection -> on_close", SAFE_SEND_NONE);
	*/

	CONNECTION_DEBUG("connection destroyed");
}

template <typename T>
typename connection<T>::socket_type &connection<T>::socket()
{
	return m_socket;
}

template <typename T>
typename connection<T>::endpoint_type &connection<T>::endpoint()
{
	return m_endpoint;
}

template <typename T>
void connection<T>::start(const std::string &local_endpoint)
{
	m_access_local = local_endpoint;
	m_access_remote = boost::lexical_cast<std::string>(m_endpoint);

	++m_server->m_data->connections_counter;

	CONNECTION_INFO("connection to client opened")
		("local", m_access_local)
		("remote", m_access_remote);

	async_read();
}

template <typename T>
void connection<T>::send_headers(http_response &&rep,
	const boost::asio::const_buffer &content,
	std::function<void (const boost::system::error_code &err)> &&handler)
{
	m_access_status = rep.code();

	if (!m_keep_alive) {
		// if connection cannot be reused, send "Connection: Close"
		rep.headers().set_keep_alive(false);
	}
	else if (auto keep_alive = rep.headers().is_keep_alive()) {
		// connection is reusable, but handler set Connection header explicitly
		// let's just use it
		m_keep_alive = *keep_alive;
	}

	CONNECTION_DEBUG("handler sends headers to client")
		("keep_alive", m_keep_alive)
		("status", rep.code())
		("state", make_state_attribute());

	auto response_buffers = rep.to_buffers();
	response_buffers.push_back(content);

	buffer_info info(
		std::move(response_buffers),
		std::move(rep),
		std::move(handler)
	);
	send_impl(std::move(info));
}

template <typename T>
void connection<T>::send_data(const boost::asio::const_buffer &buffer,
	std::function<void (const boost::system::error_code &)> &&handler)
{
	CONNECTION_DEBUG("handler sends data to client")
		("size", boost::asio::buffer_size(buffer))
		("state", make_state_attribute());

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
void connection<T>::pause_receive()
{
	m_pause_receive = true;
}

template <typename T>
void connection<T>::initialize(base_request_stream_data *data)
{
	(void) data;
}

template <typename T>
swarm::logger connection<T>::create_logger()
{
	return swarm::logger(m_logger, blackhole::log::attributes_t());
}

template <typename T>
void connection<T>::close(const boost::system::error_code &err)
{
	m_close_invoked = true;

	CONNECTION_DEBUG("handler asks for closing connection")
		("error", err.message())
		("state", make_state_attribute());

	// Invoke close_impl some time later, so we won't need any mutexes to guard the logic

	if (err) {
		m_socket.get_io_service().dispatch(std::bind(&connection::close_impl, this->shared_from_this(), err));
	} else {
		send_data(boost::asio::const_buffer(),
			std::bind(&connection::close_impl, this->shared_from_this(), std::placeholders::_1));
	}
}

template <typename T>
void connection<T>::virtual_hook(reply_stream::reply_stream_hook id, void *data)
{
	switch (id) {
	case get_logger_attributes_hook: {
		auto &attributes_data = *reinterpret_cast<get_logger_attributes_hook_data *>(data);
		attributes_data.data = &m_attributes;
		break;
	}
	}
}

template <typename T>
std::shared_ptr<base_request_stream> connection<T>::try_handler()
{
	if (!m_close_invoked)
		return m_handler;
	else
		return std::shared_ptr<base_request_stream>();
}

template <typename T>
void connection<T>::want_more_impl()
{
	CONNECTION_DEBUG("handler asks for more data from client")
		("state", make_state_attribute());

	// when connection is in processing_request state it means
	// that all data is received and all handler's callbacks are called,
	// thus, handler got everything.
	if (m_state == processing_request) {
		return;
	}

	m_pause_receive = false;

	if (m_content_length > 0 && m_unprocessed_begin == m_unprocessed_end) {
		async_read();
	}
	else {
		process_data();
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
}

template <typename T>
void connection<T>::write_finished(const boost::system::error_code &err, size_t bytes_written,
		struct timespec start_time)
{
	auto write_time = gettime_now() - start_time;
	m_send_time += write_time;
	m_access_sent += bytes_written;

	CONNECTION_LOG(err ? SWARM_LOG_ERROR : SWARM_LOG_DEBUG, "write to client finished")
		("error", err.message())
		("size", bytes_written)
		("time", timespec_to_usec(write_time));

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

		if (auto handler = try_handler()) {
			SAFE_CALL(handler->on_close(err), "connection::write_finished -> on_close", SAFE_SEND_NONE);
		}

		if (m_handler) {
			--m_server->m_data->active_connections_counter;
			m_handler.reset();
		}

		m_access_status = 499;

		close_impl(err);
	}
	else {
		std::unique_lock<std::mutex> lock(m_outgoing_mutex);

		while (!m_outgoing.empty()) {
			auto &buffers = m_outgoing.front().buffer;

			auto it = buffers.begin();

			for (; it != buffers.end(); ++it) {
				const size_t size = boost::asio::buffer_size(*it);
				if (size <= bytes_written) {
					// the whole buffer was sent
					bytes_written -= size;
				} else {
					// the buffer was sent partially, none of the following buffers
					// were sent
					*it = bytes_written + *it;
					bytes_written = 0;
					break;
				}
			}

			if (it == buffers.end()) {
				// the current buffer_info was fully sent, continue processing
				const auto handler = std::move(m_outgoing.front().handler);
				m_outgoing.pop_front();
				if (handler) {
					lock.unlock();
					handler(err);
					lock.lock();
				}
			} else {
				// the current buffer_info was sent only partially, processing should
				// be stopped
				buffers.erase(buffers.begin(), it);
				break;
			}
		}

		if (bytes_written) {
			CONNECTION_ERROR("wrote extra bytes")
				("size", bytes_written)
				("state", make_state_attribute());
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

	auto send_start = gettime_now();

	m_socket.async_write_some(data, detail::attributes_bind(m_logger, m_attributes, std::bind(
		&connection::write_finished, this->shared_from_this(),
		std::placeholders::_1, std::placeholders::_2, send_start)));
}

template <typename T>
void connection<T>::close_impl(const boost::system::error_code &err)
{
	CONNECTION_DEBUG("handler closes connection")
		("error", err.message())
		("keep_alive", m_keep_alive)
		("unreceived_size", m_content_length)
		("state", make_state_attribute());

	if (m_handler) {
		--m_server->m_data->active_connections_counter;
		m_handler.reset();
	}
	m_request_processing_was_finished = true;

	if (err) {
		// If access status is set to 499 there was an error during writing the data,
		// so it looks like the client is already dead.
		if (m_access_status != 499)
			m_access_status = 599;
		print_access_log();

		boost::system::error_code ignored_ec;
		// If there was any error - close the connection, it's broken
		m_socket.shutdown(boost::asio::socket_base::shutdown_both, ignored_ec);
		m_socket.close(ignored_ec);
		return;
	}

	if (!m_keep_alive) {
		print_access_log();
		boost::system::error_code ignored_ec;
		m_socket.shutdown(boost::asio::socket_base::shutdown_both, ignored_ec);
		m_socket.close(ignored_ec);
		return;
	}

	// Is request data is not fully received yet - receive it
	if (m_state != processing_request) {
		m_state |= request_processed;

		want_more_impl();
		return;
	}

	process_next();
}

template <typename T>
void connection<T>::process_next()
{
	print_access_log();

	// Start to wait new HTTP requests by this socket due to HTTP 1.1
	m_state = read_headers | waiting_for_first_data;
	m_access_method.clear();
	m_access_url.clear();
	m_access_start.tv_sec = 0;
	m_access_start.tv_usec = 0;
	m_access_status = 0;
	m_access_received = 0;
	m_access_sent = 0;
	m_request_processing_was_finished = false;
	m_request_parser.reset();
	m_access_log_printed = false;
	m_close_invoked = false;
	m_content_length = 0;
	m_pause_receive = false;

	m_receive_time = {0, 0};
	m_send_time = {0, 0};
	m_starttransfer_time = {0, 0};

	m_attributes.clear();
	m_logger = swarm::logger(m_base_logger, m_attributes);
	m_request = http_request();

	CONNECTION_INFO("process next request")
		("size", m_unprocessed_end - m_unprocessed_begin)
		("local", m_access_local)
		("remote", m_access_remote)
		;

	if (m_unprocessed_begin != m_unprocessed_end) {
		process_data();
	} else {
		async_read();
	}
}

template <typename T>
void connection<T>::print_access_log()
{
	if (m_state & waiting_for_first_data)
		return;

	if (m_access_log_printed)
		return;
	m_access_log_printed = true;

	timeval end;
	gettimeofday(&end, NULL);

	unsigned long long delta = 1000000ull * (end.tv_sec - m_access_start.tv_sec) + end.tv_usec - m_access_start.tv_usec;

	CONNECTION_LOG(SWARM_LOG_INFO, "access_log_entry: method: %s, url: %s, local: %s, remote: %s, status: %d, received: %llu, sent: %llu, time: %llu us, "
			"receive_time: %ld us, send_time: %ld us, starttransfer_time: %ld us",
		m_access_method.empty() ? "-" : m_access_method.c_str(),
		m_access_url.empty() ? "-" : m_access_url.c_str(),
		m_access_local.c_str(),
		m_access_remote.c_str(),
		m_access_status,
		m_access_received,
		m_access_sent,
		delta,
		timespec_to_usec(m_receive_time),
		timespec_to_usec(m_send_time),
		timespec_to_usec(m_starttransfer_time));
}

template <typename T>
void connection<T>::handle_read(const boost::system::error_code &err, std::size_t bytes_transferred,
		struct timespec start_time)
{
	auto read_time = gettime_now() - start_time;

	if (m_state & waiting_for_first_data) {
		m_starttransfer_time = read_time;
	}
	else {
		m_receive_time += read_time;
	}

	m_at_read = false;

	// This message is not error in case of disconnect between requests
	const bool error = err && !((m_state & waiting_for_first_data)
		&& err.category() == boost::asio::error::get_misc_category()
		&& err.value() == boost::asio::error::eof);

	CONNECTION_LOG(error ? SWARM_LOG_ERROR : SWARM_LOG_DEBUG, "received new data")
		("error", err.message())
		("real_error", error)
		("state", make_state_attribute())
		("size", bytes_transferred)
		("time", timespec_to_usec(read_time));

	if (err) {
		if (m_access_status == 0 || !m_request_processing_was_finished) {
			m_access_status = 499;
		}

		print_access_log();

		if (auto handler = try_handler()) {
			SAFE_CALL(handler->on_close(err), "connection::handle_read -> on_close", SAFE_SEND_NONE);
		}

		if (m_handler) {
			--m_server->m_data->active_connections_counter;
			m_handler.reset();
		}

		close_impl(err);
		return;
	}

	m_unprocessed_begin = m_buffer.data();
	m_unprocessed_end = m_buffer.data() + bytes_transferred;
	process_data();

	// If an error occurs then no new asynchronous operations are started. This
	// means that all shared_ptr references to the connection object will
	// disappear and the object will be destroyed automatically after this
	// handler returns. The connection class's destructor closes the socket.
}

template <typename T>
void connection<T>::process_data()
{
	if (m_pause_receive) {
		return;
	}

	const char* begin = m_unprocessed_begin;
	const char* end = m_unprocessed_end;

	CONNECTION_DEBUG("process data")
		("size", end - begin)
		("state", make_state_attribute());

	if (m_state & read_headers) {
		if (m_state & waiting_for_first_data) {
			m_state &= ~waiting_for_first_data;
			gettimeofday(&m_access_start, NULL);
		}

		boost::tribool result;
		const char *new_begin = NULL;
		boost::tie(result, new_begin) = m_request_parser.parse(m_request, begin, end);

		CONNECTION_DEBUG("processed headers")
			("result", result ? "true" : (!result ? "false" : "unknown_state"))
			("raw_data", std::string(begin, new_begin));

		m_access_received += (new_begin - begin);
		m_unprocessed_begin = new_begin;

		if (!result) {
			send_error(http_response::bad_request);
			return;
		} else if (result) {
			m_access_method = m_request.method();
			m_access_url = m_request.url().original();
			uint64_t request_id = 0;
			bool trace_bit = false;

			bool failed_to_parse_request_id = true;
			const std::string &request_header = m_server->m_data->request_header;
			int request_header_err = 0;

			if (!request_header.empty()) {
				if (auto request_ptr = m_request.headers().get(request_header)) {
					std::string tmp = request_ptr->substr(0, 16);
					errno = 0;
					request_id = strtoull(tmp.c_str(), NULL, 16);
					request_header_err = -errno;
					if (request_header_err != 0) {
						request_id = 0;
					} else {
						failed_to_parse_request_id = false;
					}
				}
			}

			if (failed_to_parse_request_id) {
				unsigned char *buffer = reinterpret_cast<unsigned char *>(&request_id);
				for (size_t i = 0; i < sizeof(request_id) / sizeof(unsigned char); ++i) {
					buffer[i] = std::rand();
				}
			}

			const std::string &trace_header = m_server->m_data->trace_header;
			if (!trace_header.empty()) {
				if (auto trace_bit_ptr = m_request.headers().get(trace_header)) {
					try {
						trace_bit = boost::lexical_cast<uint32_t>(*trace_bit_ptr) > 0;
					} catch (std::exception &exc) {
						CONNECTION_ERROR("failed to parse trace header, must be either 0 or 1")
							("url", m_request.url().original())
							("header_value", *trace_bit_ptr)
							("header_name", trace_header)
							("error", exc.what());
					}
				}
			}

			m_attributes = blackhole::log::attributes_t({
				swarm::keyword::request_id() = request_id,
				blackhole::keyword::tracebit() = trace_bit
			});
			m_logger = swarm::logger(m_base_logger, m_attributes);

			blackhole::scoped_attributes_t logger_guard(m_logger, blackhole::log::attributes_t(m_attributes));

			if (request_header_err != 0) {
				auto request_ptr = m_request.headers().get(request_header);

				CONNECTION_ERROR("failed to parse request header")
					("url", m_request.url().original())
					("header_value", *request_ptr)
					("header_name", request_header)
					("error", request_header_err);
			}

			m_request.set_request_id(request_id);
			m_request.set_trace_bit(trace_bit);
			m_request.set_local_endpoint(m_access_local);
			m_request.set_remote_endpoint(m_access_remote);

			if (!m_request.url().is_valid()) {
				CONNECTION_ERROR("failed to parse invalid url")
					("url", m_access_url);

				// terminate connection on invalid url
				send_error(http_response::bad_request);
				return;
			} else {
				CONNECTION_INFO(
					"received new request: method: %s, url: %s, local: %s, remote: %s, headers: %s",
					m_access_method.empty() ? "-" : m_access_method,
					m_access_url.empty() ? "-" : m_access_url,
					m_access_local,
					m_access_remote,
					headers_to_string(m_request.headers(), m_server->m_data->log_request_headers)
				);

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
					SAFE_CALL(m_handler->on_headers(std::move(m_request)), "connection::process_data -> on_headers", SAFE_SEND_ERROR);
				} else {
					CONNECTION_ERROR("failed to find handler")
						("method", m_access_method)
						("url", m_access_url);

					// terminate connection if appropriate handler is not found
					send_error(http_response::not_found);
					return;
				}
			}

			m_state &= ~read_headers;
			m_state |=  read_data;

			process_data();
			// async_read is called by processed_data
			return;
		} else {
			// need more data for request processing
			async_read();
		}
	} else if (m_state & read_data) {
		size_t data_from_body = std::min<size_t>(m_content_length, end - begin);
		size_t processed_size = data_from_body;

		if (data_from_body) {
			if (auto handler = try_handler()) {
				SAFE_CALL(processed_size = handler->on_data(boost::asio::buffer(begin, data_from_body)),
					"connection::process_data -> on_data", SAFE_SEND_ERROR);
			}
		}

		if (processed_size > data_from_body) {
			processed_size = data_from_body;
		}

		m_content_length -= processed_size;
		m_access_received += processed_size;
		m_unprocessed_begin = begin + processed_size;

		CONNECTION_DEBUG("processed body")
			("size", processed_size)
			("total_size", data_from_body)
			("need_size", m_content_length)
			("unprocesed_size", m_unprocessed_end - m_unprocessed_begin)
			("state", make_state_attribute());

		if (m_pause_receive) {
			// Handler don't want to receive more data (and callbacks),
			// wait until want_more method is called
			return;
		}

		if (data_from_body != processed_size) {
			// Handler can't process all data, wait until want_more method is called
			return;
		} else if (m_content_length > 0) {
			async_read();
		} else {
			m_state &= ~read_data;

			if (auto handler = try_handler()) {
				SAFE_CALL(handler->on_close(boost::system::error_code()), "connection::process_data -> on_close", SAFE_SEND_ERROR);
			}

			if (m_handler) {
				--m_server->m_data->active_connections_counter;
				m_handler.reset();
			}

			if (m_state & request_processed) {
				process_next();
			}
		}
	}
}

template <typename T>
void connection<T>::async_read()
{
	// here m_pause_receive is false

	if (m_at_read)
		return;

	m_at_read = true;
	m_unprocessed_begin = NULL;
	m_unprocessed_end = NULL;

	CONNECTION_DEBUG("request read from client")
		("state", make_state_attribute());

	auto receive_start = gettime_now();

	m_socket.async_read_some(boost::asio::buffer(m_buffer),
		detail::attributes_bind(m_logger, m_attributes,
			std::bind(&connection::handle_read, this->shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2,
				receive_start)));
}

template <typename T>
void connection<T>::send_error(http_response::status_type type)
{
	CONNECTION_DEBUG("handler sends error to client")
		("status", type)
		("state", make_state_attribute());

	http_response response;
	response.set_code(type);
	response.headers().set_content_length(0);
	response.headers().set_keep_alive(false);

	send_headers(std::move(response), boost::asio::const_buffer(), result_function());
	close(boost::system::error_code());
}

template class connection<boost::asio::local::stream_protocol::socket>;
template class connection<boost::asio::ip::tcp::socket>;

} // namespace thevoid
} // namespace ioremap
