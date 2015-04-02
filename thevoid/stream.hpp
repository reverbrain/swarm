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

#include "http_request.hpp"
#include "http_response.hpp"
#include <swarm/logger.hpp>
#include <boost/asio.hpp>
#include <cstdarg>
#include <type_traits>
#include <blackhole/utils/atomic.hpp>
#include <blackhole/scoped_attributes.hpp>

namespace ioremap {
namespace thevoid {

namespace detail {
template <typename Method>
struct attributes_bind_handler
{
	swarm::logger *logger;
	blackhole::log::attributes_t *attributes;
	Method method;

	template <typename... Args>
	void operator() (Args &&...args)
	{
		blackhole::scoped_attributes_t logger_guard(*logger, blackhole::log::attributes_t(*attributes));
		method(std::forward<Args>(args)...);
	}
};

template <typename Method>
attributes_bind_handler<typename std::remove_reference<Method>::type> attributes_bind(
	swarm::logger &logger, blackhole::log::attributes_t &attributes, Method &&method)
{
	return {
		&logger,
		&attributes,
		std::forward<Method>(method)
	};
}
}

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
	typedef std::function<void (const boost::system::error_code &err)> result_function;

	enum reply_stream_hook
	{
		get_logger_attributes_hook
	};

	struct get_logger_attributes_hook_data
	{
		blackhole::log::attributes_t *data;
	};

	reply_stream();
	virtual ~reply_stream();

	/*!
	 * \brief Sends headers \a rep with \a content to client.
	 *
	 * At finish \a handler is called with error_code.
	 */
	virtual void send_headers(http_response &&rep, const boost::asio::const_buffer &content, result_function &&handler) = 0;
	/*!
	 * \brief Sends raw data \a buffer to client.
	 *
	 * At finish \a handler is called with error_code.
	 */
	virtual void send_data(const boost::asio::const_buffer &buffer, result_function &&handler) = 0;
	/*!
	 * \brief Tell event loop to read more data from socket.
	 *
	 * This method may be used for asynchronous processing of incoming data by chunks.
	 *
	 * After this call method base_request_stream::on_data is called as soon as data arrives.
	 */
	virtual void want_more() = 0;
	/*!
	 * \brief Pauses processing data until want_more() method is called.
	 *
	 * This is method is not thread-safe and may be called from within
	 * base_request_stream::on_headers() and base_request_stream::on_data() methods.
	 *
	 * If this method is called from within base_request_stream::on_headers() method
	 * following base_request_stream::on_data() method's call is postponed until
	 * want_more() method is called.
	 *
	 * Analogously, calling this method from within base_request_stream::on_data()
	 * method will postpone following base_request_stream::on_data() or
	 * base_request_stream::on_close() method's call.
	 *
	 * However, if error happens base_request_stream::on_close() with corresponding
	 * error_code will be called.
	 */
	virtual void pause_receive() = 0;
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

	virtual swarm::logger create_logger() = 0;

	virtual void virtual_hook(reply_stream_hook id, void *data);

	blackhole::log::attributes_t *get_logger_attributes();
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
	typedef reply_stream::result_function result_function;

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

	/*!
	 * \brief Returns the logger.
	 */
	const swarm::logger &logger()
	{
		if (__builtin_expect(!m_logger, false))
			throw std::logic_error("request_stream::m_logger is null");

		return *m_logger;
	}

	virtual void virtual_hook(request_stream_hook id, void *data);

	template <typename Method>
	detail::attributes_bind_handler<typename std::remove_reference<Method>::type> wrap(Method handler)
	{
		return detail::attributes_bind(*m_logger, *logger_attributes(), std::move(handler));
	}

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

private:
	blackhole::log::attributes_t *logger_attributes();

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
		reply()->send_headers(std::move(rep), boost::asio::const_buffer(), result_function());
		reply()->close(boost::system::error_code());
	}

	/*!
	 * \brief Sends \a rep with \a data to client and closes the stream.
	 */
	template <typename T>
	void send_reply(http_response &&rep, T &&data)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		auto wrapper = make_wrapper(std::forward<T>(data), result_function());
		auto buffer = cast_to_buffer(wrapper);
		reply()->send_headers(std::move(rep), buffer, std::move(wrapper));
		reply()->close(boost::system::error_code());
	}

	/*!
	 * \brief Sends reply with status \a code to client and closes the stream.
	 */
	void send_reply(int code)
	{
		http_response response;
		response.set_code(code);
		response.headers().set_content_length(0);

		send_reply(std::move(response));
	}

	/*!
	 * \brief Sends response \a rep to client, calls \a handler with result.
	 *
	 * \sa reply_stream::send_headers
	 */
	void send_headers(http_response &&rep, result_function &&handler)
	{
		reply()->send_headers(std::move(rep), boost::asio::const_buffer(), std::move(handler));
	}

	/*!
	 * \brief Sends response \a rep with \a data to client and calls \a handler with result.
	 *
	 * \sa reply_stream::send_headers
	 */
	template <typename T>
	void send_headers(http_response &&rep, T &&data, result_function &&handler)
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
	void send_data(const boost::asio::const_buffer &data, result_function &&handler)
	{
		reply()->send_data(data, std::move(handler));
	}

	/*!
	 * \brief Sends raw \a data to client and calls \a handler with result.
	 *
	 * \sa reply_stream::send_data
	 */
	template <typename T>
	void send_data(T &&data, result_function &&handler)
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
		result_function m_handler;

		functor_wrapper(T &&data, result_function &&handler)
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
	functor_wrapper<typename std::remove_reference<T>::type> make_wrapper(T &&data, result_function &&handler)
	{
		static_assert(std::is_rvalue_reference<decltype(data)>::value, "data must be rvalue");
		return functor_wrapper<typename std::remove_reference<T>::type>(std::forward<T>(data), std::move(handler));
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
 * One must call try_next_chunk() to receive first chunk.
 *
 * One MUST NOT call reply_stream::pause_receive() from within on_chunk() or on_request()
 * methods. Next on_chunk() method's call is already posponed until try_next_chunk() method
 * is called.
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
		m_chunk_size(10 * 1024), m_client_asked_chunk(false),
		m_first_chunk(true), m_last_chunk(false),
		m_unprocessed_size(0)
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
	 */
	void try_next_chunk()
	{
		// mark handler's readiness to process next chunk
		m_client_asked_chunk = true;

		/*
		 * want_more() will produce on_data() call from within io_service's thread pool.
		 * Thus, on_chunk() method will be called from within single thread,
		 * while try_next_chunk() method may be called from within user's threads.
		 */
		this->reply()->want_more();
	}

private:
	/*!
	 * \internal
	 */
	void on_headers(http_request &&req)
	{
		m_unprocessed_size = req.headers().content_length().get_value_or(0);
		m_request = std::move(req);

		on_request(m_request);

		m_data.reserve(m_chunk_size);
	}

	/*!
	 * \internal
	 */
	size_t on_data(const boost::asio::const_buffer &buffer)
	{
		auto begin = boost::asio::buffer_cast<const char *>(buffer);
		auto size = boost::asio::buffer_size(buffer);

		size_t buffered_size = 0;

		// Loop invariant is m_data is not ready to be processed at the beginning of each iteration.
		while (size) {
			// The size that we need to pass to the client
			const auto real_chunk_size = std::min(m_unprocessed_size, m_chunk_size);
			const auto delta = std::min(size, real_chunk_size - m_data.size());

			if (delta) {
				buffered_size += delta;
				m_data.insert(m_data.end(), begin, begin + delta);
				begin += delta;
				size -= delta;
			}

			// We will call on_chunk if both conditions are true
			// 1. Client asked next chunk
			// 2. The chunk is ready to be processed
			// The second condition means either chunk is full or chunk contains last data
			if (m_data.size() == real_chunk_size) {
				if (m_client_asked_chunk) {
					process_chunk_internal();
				}
				else {
					// chunk is ready but client is not
					if (size == 0) {
						this->reply()->pause_receive();
					}
					break;
				}
			}
		}

		return buffered_size;
	}

	/*!
	 * \internal
	 */
	void on_close(const boost::system::error_code &err)
	{
		if (err) {
			on_error(err);
			return;
		}

		if (m_unprocessed_size) {
			process_chunk_internal();
		}
	}

	/*!
	 * \internal
	 *
	 * process_chunk_internal() will be called only if both chunk and client
	 * are ready for processing (m_client_asked_chunk is true).
	 */
	void process_chunk_internal()
	{
		int flags = 0;
		if (m_first_chunk)
			flags |= first_chunk;
		m_first_chunk = false;

		/*
		 * last_chunk logic must rely on actual unprocessed size,
		 * not on on_close() method call because on_close() method might be called after
		 * "last chunk" processing (if content length is multiple of chunk size).
		 */
		m_unprocessed_size -= m_data.size();
		if (!m_unprocessed_size) {
			m_last_chunk = true;
			flags |= last_chunk;
		}

		/*
		 * m_client_asked_chunk must be cleared before on_chunk() call
		 * because handler is allowed to call try_next_chunk() at the end of on_chunk() method
		 * (in case of synchronous chunk processing)
		 * and mark handler's readiness for processing next chunk.
		 */
		m_client_asked_chunk = false;

		on_chunk(boost::asio::buffer(m_data), flags);

		m_data.resize(0);
	}

	http_request m_request;
	std::vector<char> m_data;
	size_t m_chunk_size;
	std::atomic<bool> m_client_asked_chunk;
	bool m_first_chunk;
	bool m_last_chunk;
	size_t m_unprocessed_size;
};

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_STREAM_HPP
