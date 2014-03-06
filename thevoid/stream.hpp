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

#ifndef IOREMAP_THEVOID_STREAM_HPP
#define IOREMAP_THEVOID_STREAM_HPP

#include <boost/asio.hpp>
#include <swarm/http_request.hpp>
#include <swarm/http_response.hpp>
#include <swarm/logger.hpp>
#include <swarm/c++config.hpp>
#include <cstdarg>
#include <type_traits>

#ifdef SWARM_CSTDATOMIC
#  include <cstdatomic>
#else
#  include <atomic>
#endif

namespace ioremap {
namespace thevoid {

class reply_stream
{
public:
	reply_stream();
	virtual ~reply_stream();

	virtual void send_headers(swarm::http_response &&rep,
				  const boost::asio::const_buffer &content,
				  std::function<void (const boost::system::error_code &err)> &&handler) = 0;
	virtual void send_data(const boost::asio::const_buffer &buffer,
			       std::function<void (const boost::system::error_code &err)> &&handler) = 0;
	virtual void want_more() = 0;
	virtual void close(const boost::system::error_code &err) = 0;

	virtual void send_error(swarm::http_response::status_type type) = 0;
};

class base_request_stream
{
public:
	base_request_stream();
	virtual ~base_request_stream();

	virtual void on_headers(swarm::http_request &&req) = 0;
	virtual size_t on_data(const boost::asio::const_buffer &buffer) = 0;
	virtual void on_close(const boost::system::error_code &err) = 0;

	void initialize(const std::shared_ptr<reply_stream> &reply);

protected:
	std::shared_ptr<reply_stream> get_reply()
	{
		if (__builtin_expect(!m_reply, false))
			throw std::logic_error("request_stream::m_reply is null");

		return m_reply;
	}

private:
	std::shared_ptr<reply_stream> m_reply;
};

template <typename Server>
class request_stream : public base_request_stream
{
public:
	request_stream() {}
	virtual ~request_stream() {}

	void set_server(const std::shared_ptr<Server> &server)
	{
		m_server = server;
	}

protected:
	std::shared_ptr<Server> server()
	{
		if (__builtin_expect(!m_server, false))
			throw std::logic_error("request_stream::m_server must be initialized");
		return m_server;
	}

	swarm::logger logger()
	{
		return server()->logger();
	}

	void log(int level, const char *format, ...) __attribute__ ((format(printf, 3, 4)))
	{
		va_list args;
		va_start(args, format);

		logger().vlog(level, format, args);

		va_end(args);
	}

	void send_reply(swarm::http_response &&rep)
	{
		get_reply()->send_headers(std::move(rep), boost::asio::const_buffer(), make_close_handler());
	}

	template <typename T>
	void send_reply(swarm::http_response &&rep, T &&data)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		auto wrapper = make_wrapper(std::forward<T>(data), make_close_handler());
		auto buffer = boost::asio::buffer(wrapper.data());
		get_reply()->send_headers(std::move(rep), buffer, std::move(wrapper));
	}

	void send_reply(int code)
	{
		get_reply()->send_error(static_cast<swarm::http_response::status_type>(code));
	}

	void send_headers(swarm::http_response &&rep,
			  std::function<void (const boost::system::error_code &err)> &&handler)
	{
		get_reply()->send_headers(std::move(rep), boost::asio::const_buffer(), std::move(handler));
	}

	template <typename T>
	void send_headers(swarm::http_response &&rep,
			  T &&data,
			  std::function<void (const boost::system::error_code &err)> &&handler)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		auto wrapper = make_wrapper(std::forward<T>(data), std::move(handler));
		auto buffer = boost::asio::buffer(wrapper.data());
		get_reply()->send_headers(std::move(rep), buffer, std::move(wrapper));
	}

	void send_data(const boost::asio::const_buffer &data,
		       std::function<void (const boost::system::error_code &err)> &&handler)
	{
		get_reply()->send_data(data, std::move(handler));
	}

	template <typename T>
	void send_data(T &&data,
		       std::function<void (const boost::system::error_code &err)> &&handler)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		auto wrapper = make_wrapper(std::forward<T>(data), std::move(handler));
		auto buffer = boost::asio::buffer(wrapper.data());
		get_reply()->send_data(buffer, std::move(wrapper));
	}

	void close(const boost::system::error_code &err)
	{
		get_reply()->close(err);
	}

private:
	template <typename T>
	struct functor_wrapper
	{
		std::shared_ptr<T> m_data;
		std::function<void (const boost::system::error_code &err)> m_handler;

		functor_wrapper(T &&data, std::function<void (const boost::system::error_code &err)> &&handler)
			: m_data(std::make_shared<T>(std::forward<T>(data))), m_handler(std::move(handler))
		{
		}

		void operator() (const boost::system::error_code &err) const
		{
			if (m_handler)
				m_handler(err);
		}

		T &data()
		{
			return *m_data;
		}
	};

	template <typename T>
	functor_wrapper<typename std::remove_reference<T>::type> make_wrapper(T &&data, std::function<void (const boost::system::error_code &err)> &&handler)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		return functor_wrapper<typename std::remove_reference<T>::type>(std::forward<T>(data), std::move(handler));
	}

	std::function<void (const boost::system::error_code &err)> make_close_handler()
	{
		return std::move(std::bind(&reply_stream::close, get_reply(), std::placeholders::_1));
	}

	std::shared_ptr<Server> m_server;
};

template <typename Server>
class simple_request_stream : public request_stream<Server>
{
public:
	virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) = 0;

protected:
	const swarm::http_request &request()
	{
		return m_request;
	}

private:
	void on_headers(swarm::http_request &&req)
	{
		m_request = std::move(req);
		if (auto tmp = req.headers().content_length())
			m_data.reserve(*tmp);
	}

	size_t on_data(const boost::asio::const_buffer &buffer)
	{
		auto begin = boost::asio::buffer_cast<const char *>(buffer);
		auto size = boost::asio::buffer_size(buffer);
		m_data.insert(m_data.end(), begin, begin + size);
		return size;
	}

	void on_close(const boost::system::error_code &err)
	{
		if (!err) {
			on_request(m_request, boost::asio::buffer(m_data.data(), m_data.size()));
		}
	}

	swarm::http_request m_request;
	std::vector<char> m_data;
};

template <typename Server>
class buffered_request_stream : public request_stream<Server>
{
public:
	enum chunk_flags {
		first_chunk = 0x01,
		last_chunk = 0x02,
		single_chunk = first_chunk | last_chunk
	};

	buffered_request_stream() :
		m_chunk_size(10 * 1024), m_state(1),
		m_first_chunk(true), m_last_chunk(false)
	{
	}

	virtual void on_request(const swarm::http_request &req) = 0;
	virtual void on_chunk(const boost::asio::const_buffer &buffer, unsigned int flags) = 0;
	virtual void on_error(const boost::system::error_code &err) = 0;

protected:
	const swarm::http_request &request()
	{
		return m_request;
	}

	void set_chunk_size(size_t chunk_size)
	{
		m_chunk_size = chunk_size;
	}

	size_t chunk_size() const
	{
		return m_chunk_size;
	}

	void try_next_chunk()
	{
		this->try_next_chunk_internal(1);
	}

private:
	void on_headers(swarm::http_request &&req)
	{
		m_request = std::move(req);

		on_request(m_request);

		m_data.reserve(m_chunk_size);
	}

	size_t on_data(const boost::asio::const_buffer &buffer)
	{
		if (m_state & 2)
			return 0;

		auto begin = boost::asio::buffer_cast<const char *>(buffer);
		auto size = boost::asio::buffer_size(buffer);
		const auto original_size = size;

		while (size > 0) {
			const auto delta = std::min(size, m_chunk_size - m_data.size());
			if (delta == 0) {
				// We already called try_next_chunk, don't need to call it second time
				return original_size - size;
			}

			m_data.insert(m_data.end(), begin, begin + delta);
			begin += delta;
			size -= delta;

			if (m_data.size() == m_chunk_size) {
				this->try_next_chunk_internal(2);
				return original_size - size;
			}
		}
		return original_size;
	}

	void on_close(const boost::system::error_code &err)
	{
		if (err) {
			on_error(err);
		} else {
			m_last_chunk = true;
			try_next_chunk_internal(2);
		}
	}

	void try_next_chunk_internal(unsigned int flag)
	{
		if ((m_state |= flag) == 3) {
			int flags = 0;
			if (m_first_chunk)
				flags |= first_chunk;
			m_first_chunk = false;
			if (m_last_chunk)
				flags |= last_chunk;

			on_chunk(boost::asio::buffer(m_data), flags);

			m_data.resize(0);
			m_state = 0;

			this->get_reply()->want_more();
		}
	}

	swarm::http_request m_request;
	std::vector<char> m_data;
	size_t m_chunk_size;
	std::atomic_uint m_state;
	bool m_first_chunk;
	bool m_last_chunk;
};

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_STREAM_HPP
