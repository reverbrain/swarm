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
#include "http_request.hpp"
#include "http_response.hpp"
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

/*!
 * \brief The buffer_traits class makes possible to add support
 * for own types' convertion to boost::asio::const_buffer.
 *
 * \code
namespace ioremap { namespace thevoid {
template <>
struct buffer_traits<elliptics::data_pointer>
{
	static boost::asio::const_buffer convert(const elliptics::data_pointer &data)
	{
		return boost::asio::const_buffer(data.data(), data.size());
	}
};
}}
 * \endcode
 */
template <typename T>
struct buffer_traits
{
	static boost::asio::const_buffer convert(const T &data)
	{
		using boost::asio::buffer;
		return buffer(data);
	}
};

class base_request_stream_data;

/*!
 * \brief The reply_stream class provides API for accessing reply stream.
 *
 * All methods of this class are thread-safe.
 */
class reply_stream
{
public:
	enum reply_stream_hook
	{
	};

	reply_stream();
	virtual ~reply_stream();

	/*!
	 * \brief Sends headers \a rep with \a content to client.
	 *
	 * At finish \a handler is called with error_code.
	 */
	virtual void send_headers(http_response &&rep,
				  const boost::asio::const_buffer &content,
				  std::function<void (const boost::system::error_code &err)> &&handler) = 0;
	/*!
	 * \brief Sends raw data \a buffer to client.
	 *
	 * At finish \a handler is called with error_code.
	 */
	virtual void send_data(const boost::asio::const_buffer &buffer,
			       std::function<void (const boost::system::error_code &err)> &&handler) = 0;
	/*!
	 * \brief Tell event loop to read more data from socket.
	 *
	 * This method may be used for asynchronous processing of incoming data by chunks.
	 *
	 * After this call method base_request_stream::on_data is called as soon as data arrives.
	 */
	virtual void want_more() = 0;
	/*!
	 * \brief Closes socket to client.
	 *
	 * If \a err is set connection to client is terminated.
	 * Otherwise if connection is keep-alive server will try wait for the next request.
	 */
	virtual void close(const boost::system::error_code &err) = 0;

	/*!
	 * \brief Send HTTP reply with status \a type.
	 *
	 * This method is a shortcut for:
	 * \code{.cpp}
	 * ioremap::http_response response(type);
	 * send_headers(std::move(response), boost::asio::const_buffer(),
	 *	std::bind(&reply_stream::close, reply, std::placeholders::_1));
	 * \endcode
	 */
	virtual void send_error(http_response::status_type type) = 0;

	virtual void initialize(base_request_stream_data *data) = 0;

	virtual void virtual_hook(reply_stream_hook id, void *data);
};

/*!
 * \brief The base_request_stream class is an interface for implementing own HTTP handlers.
 *
 * Each handler is created exactly for one request is destroyed after it's processing is finished.
 *
 * \attention It's recommended to use std::enable_shared_from_this for making possible to
 * prolong the life for the stream.
 *
 * The lifetime of request stream is the following:
 * \li server receives new HTTP request from client.
 * \li after all headers are received and parsed request_stream is created, shared pointer to it is stored in reply_stream.
 * \li shared_pointer to reply stream is stored in request_stream.
 * \li once reply_stream::close is called shared pointer to request_stream is removed from reply_stream.
 * \li reply_stream waits for new HTTP request.
 *
 * \attention Don't forget to call reply_stream::close as it will lead to memory & resources leak.
 *
 * It's always possible to see the number of reply_streams and of request_streams by monitoring.
 * It's connections and active-connections counters accordingly.
 *
 * Usually you don't need to derive from this class directly, try to use request_stream instead.
 *
 * All user's methods of this object will always be called from the same thread where this object
 * was created. All other methods are thread-safe.
 *
 * \sa reply_stream
 * \sa request_stream
 */
class base_request_stream
{
public:
	enum request_stream_hook
	{
	};

	/*!
	 * \brief Constructs the object.
	 */
	base_request_stream();
	/*!
	 * \brief Destroys the object.
	 */
	virtual ~base_request_stream();

	/*!
	 * \brief This method is called right after recieving of the headers from the client.
	 *
	 * You may store \a req anywhere in your class as it's right reference.
	 */
	virtual void on_headers(http_request &&req) = 0;
	/*!
	 * \brief This method is called at any chunk \a buffer received from the server.
	 *
	 * Returns the number of processed bytes.
	 * If returned number is not equal to buffer's size server will stop receiving data
	 * from the client until reply_stream::want_more is called.
	 *
	 * \attention You must guarantee that \a buffer will be accessable until the handler's call.
	 *
	 * \sa reply_stream::want_more
	 */
	virtual size_t on_data(const boost::asio::const_buffer &buffer) = 0;
	/*!
	 * \brief This method is called as all data from the client is received.
	 *
	 * If there is an error \a err - the error happens. In such case it's recommended
	 * to stop processing of the data and call reply_stream::close.
	 */
	virtual void on_close(const boost::system::error_code &err) = 0;

	/*!
	 * \internal
	 */
	void initialize(const std::shared_ptr<reply_stream> &reply);

	virtual void virtual_hook(request_stream_hook id, void *data);

protected:
	/*!
	 * \brief Returns pointer to reply_stream associated with this stream.
	 *
	 * \sa reply_stream
	 */
	const std::shared_ptr<reply_stream> &reply()
	{
		if (__builtin_expect(!m_reply, false))
			throw std::logic_error("request_stream::m_reply is null");

		return m_reply;
	}

	__attribute__((deprecated))
	const std::shared_ptr<reply_stream> &get_reply()
	{
		return reply();
	}

	/*!
	 * \brief Returns the logger.
	 */
	const swarm::logger &logger()
	{
		if (__builtin_expect(!m_logger, false))
			throw std::logic_error("request_stream::m_logger is null");

		return *m_logger;
	}

private:
	std::shared_ptr<reply_stream> m_reply;
	std::unique_ptr<swarm::logger> m_logger;
	std::unique_ptr<base_request_stream_data> m_data;
};

/*!
 * \brief The request_stream class is a base class for your HTTP handlers.
 *
 * Each handler is created exactly for one request is destroyed after it's processing is finished.
 *
 * \attention It's recommended to use std::enable_shared_from_this for making possible to
 * prolong the life for the stream.
 *
 * \sa base_request_stream
 * \sa simple_request_stream
 */
template <typename Server>
class request_stream : public base_request_stream
{
public:
	request_stream() {}
	virtual ~request_stream() {}

	/*!
	 * \internal
	 */
	void set_server(Server *server)
	{
		m_server = server;
	}

protected:
	/*!
	 * \brief Returns the pointer to the server.
	 */
	Server *server()
	{
		if (__builtin_expect(!m_server, false))
			throw std::logic_error("request_stream::m_server must be initialized");
		return m_server;
	}

	/*!
	 * \brief Sends \a rep to client and closes the stream.
	 */
	void send_reply(http_response &&rep)
	{
		reply()->send_headers(std::move(rep), boost::asio::const_buffer(), make_close_handler());
	}

	/*!
	 * \brief Sends \a rep with \a data to client and closes the stream.
	 */
	template <typename T>
	void send_reply(http_response &&rep, T &&data)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		auto wrapper = make_wrapper(std::forward<T>(data), make_close_handler());
		auto buffer = cast_to_buffer(wrapper);
		reply()->send_headers(std::move(rep), buffer, std::move(wrapper));
	}

	/*!
	 * \brief Sends reply with status \a code to client and closes the stream.
	 */
	void send_reply(int code)
	{
		reply()->send_error(static_cast<http_response::status_type>(code));
	}

	/*!
	 * \brief Sends response \a rep to client, calls \a handler with result.
	 *
	 * \sa reply_stream::send_headers
	 */
	void send_headers(http_response &&rep,
			  std::function<void (const boost::system::error_code &err)> &&handler)
	{
		reply()->send_headers(std::move(rep), boost::asio::const_buffer(), std::move(handler));
	}

	/*!
	 * \brief Sends response \a rep with \a data to client and calls \a handler with result.
	 *
	 * \sa reply_stream::send_headers
	 */
	template <typename T>
	void send_headers(http_response &&rep,
			  T &&data,
			  std::function<void (const boost::system::error_code &err)> &&handler)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		auto wrapper = make_wrapper(std::forward<T>(data), std::move(handler));
		auto buffer = cast_to_buffer(wrapper);
		reply()->send_headers(std::move(rep), buffer, std::move(wrapper));
	}

	/*!
	 * \brief Sends raw \a data to client and calls \a handler with result.
	 *
	 * \attention You must guarantee that \a data will be accessable until the handler's call.
	 *
	 * \sa reply_stream::send_data
	 */
	void send_data(const boost::asio::const_buffer &data,
		       std::function<void (const boost::system::error_code &err)> &&handler)
	{
		reply()->send_data(data, std::move(handler));
	}

	/*!
	 * \brief Sends raw \a data to client and calls \a handler with result.
	 *
	 * \sa reply_stream::send_data
	 */
	template <typename T>
	void send_data(T &&data,
		       std::function<void (const boost::system::error_code &err)> &&handler)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		auto wrapper = make_wrapper(std::forward<T>(data), std::move(handler));
		auto buffer = cast_to_buffer(wrapper);
		reply()->send_data(buffer, std::move(wrapper));
	}

	/*!
	 * \brief Closes the stream with error \a err.
	 *
	 * \sa reply_stream::close
	 */
	void close(const boost::system::error_code &err)
	{
		reply()->close(err);
	}

private:
	/*!
	 * \internal
	 */
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

	/*!
	 * \internal
	 */
	template <typename T>
	boost::asio::const_buffer cast_to_buffer(functor_wrapper<T> &wrapper)
	{
		return buffer_traits<T>::convert(wrapper.data());
	}

	/*!
	 * \internal
	 */
	template <typename T>
	functor_wrapper<typename std::remove_reference<T>::type> make_wrapper(T &&data, std::function<void (const boost::system::error_code &err)> &&handler)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		return functor_wrapper<typename std::remove_reference<T>::type>(std::forward<T>(data), std::move(handler));
	}

	/*!
	 * \internal
	 */
	std::function<void (const boost::system::error_code &err)> make_close_handler()
	{
		return std::move(std::bind(&reply_stream::close, reply(), std::placeholders::_1));
	}

	Server *m_server;
};

/*!
 * \brief The simple_request_stream class provides simpler interface for implementing own HTTP handlers.
 *
 * This method has the single pure-virtual method on_request, so it's usually simpler to implement own
 * handler using this API that by request_stream.
 *
 * \sa request_stream
 */
template <typename Server>
class simple_request_stream : public request_stream<Server>
{
public:
	/*!
	 * \brief This method is called as all data from the client are reveived without an error.
	 *
	 * Http request details are available by \a req, POST data by \a buffer.
	 */
	virtual void on_request(const http_request &req, const boost::asio::const_buffer &buffer) = 0;

protected:
	/*!
	 * \brief Returns const reference to ioremap::http_request associated initiated this handler.
	 */
	const http_request &request()
	{
		return m_request;
	}

private:
	/*!
	 * \internal
	 */
	void on_headers(http_request &&req)
	{
		m_request = std::move(req);
		if (auto tmp = req.headers().content_length())
			m_data.reserve(*tmp);
	}

	/*!
	 * \internal
	 */
	size_t on_data(const boost::asio::const_buffer &buffer)
	{
		auto begin = boost::asio::buffer_cast<const char *>(buffer);
		auto size = boost::asio::buffer_size(buffer);
		m_data.insert(m_data.end(), begin, begin + size);
		return size;
	}

	/*!
	 * \internal
	 */
	void on_close(const boost::system::error_code &err)
	{
		if (!err) {
			on_request(m_request, boost::asio::buffer(m_data.data(), m_data.size()));
		}
	}

	http_request m_request;
	std::vector<char> m_data;
};

/*!
 * \brief The buffered_request_stream class provide API for creating buffered requests.
 *
 * In some cases it's needed to load a lot of data from the client, the common case is
 * uploading of really HUGE video file. It's usually not possible to fir into the memory
 * so this client allows to use the following scheme: read chunk, write it to server,
 * read next chunk and so on.
 *
 * Each chunk is provided to on_chunk method.
 * Next chunk is starting accumulated as soon as your on_chunk method is called.
 *
 * After chunk's processing is finished call try_next_chunk.
 *
 * \sa set_chunk_size
 */
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

	/*!
	 * \brief This method is called as headers \a req are received.
	 */
	virtual void on_request(const http_request &req) = 0;
	/*!
	 * \brief This method is called on every received chunk \a buffer from client.
	 *
	 * \a flags contains information if this is the last or the first chunk.
	 *
	 * \sa chunk_flags
	 */
	virtual void on_chunk(const boost::asio::const_buffer &buffer, unsigned int flags) = 0;
	/*!
	 * \brief This method is called if connection was terminated by the error \a err.
	 */
	virtual void on_error(const boost::system::error_code &err) = 0;

protected:
	/*!
	 * \brief Returns the request initiated this handler.
	 */
	const http_request &request()
	{
		return m_request;
	}

	/*!
	 * \brief Sets size of received chunks.
	 *
	 * Default size is 10 kilobytes.
	 *
	 * \sa chunk_size
	 */
	void set_chunk_size(size_t chunk_size)
	{
		m_chunk_size = chunk_size;
	}

	/*!
	 * \brief Returns size of chunks.
	 *
	 * \sa set_chunk_size
	 */
	size_t chunk_size() const
	{
		return m_chunk_size;
	}

	/*!
	 * \brief Tells the server that handler is ready to process next chunk.
	 *
	 * Call it after you finished processing of previous chunk.
	 *
	 * \attention It may produce immidiate call of on_chunk's method.
	 */
	void try_next_chunk()
	{
		this->try_next_chunk_internal(1);
	}

private:
	/*!
	 * \internal
	 */
	void on_headers(http_request &&req)
	{
		m_request = std::move(req);

		on_request(m_request);

		m_data.reserve(m_chunk_size);
	}

	/*!
	 * \internal
	 */
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

	/*!
	 * \internal
	 */
	void on_close(const boost::system::error_code &err)
	{
		if (err) {
			on_error(err);
		} else {
			m_last_chunk = true;
			try_next_chunk_internal(2);
		}
	}

	/*!
	 * \internal
	 */
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

			this->reply()->want_more();
		}
	}

	http_request m_request;
	std::vector<char> m_data;
	size_t m_chunk_size;
	std::atomic_uint m_state;
	bool m_first_chunk;
	bool m_last_chunk;
};

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_STREAM_HPP
