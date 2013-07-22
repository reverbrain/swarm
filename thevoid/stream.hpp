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

#ifndef IOREMAP_THEVOID_STREAM_HPP
#define IOREMAP_THEVOID_STREAM_HPP

#include <boost/asio.hpp>
#include <swarm/networkrequest.h>

namespace ioremap { namespace thevoid { namespace detail {
struct network_reply_wrapper
{
	const swarm::network_reply reply;

	network_reply_wrapper(const swarm::network_reply &reply) : reply(reply)
	{
	}
};
}

} } // namespace ioremap::thevoid::detail

namespace boost { namespace asio {

inline const_buffer buffer(const ioremap::thevoid::detail::network_reply_wrapper &wrapper)
{
	return buffer(wrapper.reply.get_data());
}

} } // namespace boost::asio

namespace ioremap {
namespace thevoid {

class reply_stream
{
public:
	reply_stream();
	virtual ~reply_stream();

	virtual void send_headers(const swarm::network_reply &rep,
				  const boost::asio::const_buffer &content,
				  const std::function<void (const boost::system::error_code &err)> &handler) = 0;
	virtual void send_data(const boost::asio::const_buffer &buffer,
			       const std::function<void (const boost::system::error_code &err)> &handler) = 0;
	virtual void close(const boost::system::error_code &err) = 0;

	virtual void send_error(swarm::network_reply::status_type type) = 0;
};

class base_request_stream
{
public:
	base_request_stream();
	virtual ~base_request_stream();

	virtual void on_headers(const swarm::network_request &req) = 0;
	virtual void on_data(const boost::asio::const_buffer &buffer) = 0;
	virtual void on_close(const boost::system::error_code &err) { (void) err; }

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
	std::shared_ptr<Server> get_server()
	{
		if (__builtin_expect(!m_server, false))
			throw std::logic_error("request_stream::m_server must be initialized");
		return m_server;
	}

	void send_reply(const swarm::network_reply &rep)
	{
		send_reply(rep, detail::network_reply_wrapper(rep));
	}

	template <typename T>
	void send_reply(const swarm::network_reply &rep, T &&data)
	{
		auto wrapper = make_wrapper(std::move(data), make_close_handler());
		get_reply()->send_headers(rep, boost::asio::buffer(wrapper.data()), wrapper);
	}

	void send_reply(int code)
	{
		get_reply()->send_error(static_cast<swarm::network_reply::status_type>(code));
	}

	void send_headers(const swarm::network_reply &rep,
			  const std::function<void (const boost::system::error_code &err)> &handler)
	{
		get_reply()->send_headers(rep, detail::network_reply_wrapper(rep), handler);
	}

	template <typename T>
	void send_headers(const swarm::network_reply &rep,
			  T &&data,
			  const std::function<void (const boost::system::error_code &err)> &handler)
	{
		auto wrapper = make_wrapper(std::move(data), handler);
		get_reply()->send_headers(rep, boost::asio::buffer(wrapper.data()), wrapper);
	}

	void send_data(const boost::asio::const_buffer &data,
		       const std::function<void (const boost::system::error_code &err)> &handler)
	{
		get_reply()->send_data(data, handler);
	}

	template <typename T>
	void send_data(T &&data,
		       const std::function<void (const boost::system::error_code &err)> &handler)
	{
		auto wrapper = make_wrapper(std::move(data), handler);
		get_reply()->send_data(boost::asio::buffer(wrapper.data()), wrapper);
	}

private:
	template <typename T>
	struct functor_wrapper
	{
		std::shared_ptr<T> m_data;
		std::function<void (const boost::system::error_code &err)> m_handler;

		functor_wrapper(T &&data, const std::function<void (const boost::system::error_code &err)> &handler)
			: m_data(std::make_shared<T>(std::move(data))), m_handler(handler)
		{
		}

		void operator() (const boost::system::error_code &err) const
		{
			m_handler(err);
		}

		T &data()
		{
			return *m_data;
		}
	};

	template <typename T>
	functor_wrapper<T> make_wrapper(T &&data, const std::function<void (const boost::system::error_code &err)> &handler)
	{
		return functor_wrapper<T>(std::move(data), handler);
	}

	std::function<void (const boost::system::error_code &err)> make_close_handler()
	{
		return std::bind(&reply_stream::close, get_reply(), std::placeholders::_1);
	}

	std::shared_ptr<Server> m_server;
};

template <typename Server>
class simple_request_stream : public request_stream<Server>
{
public:
	virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) = 0;

protected:
	const swarm::network_request &get_request()
	{
		return m_request;
	}

private:
	void on_headers(const swarm::network_request &req)
	{
		m_request = req;
		m_content_length = req.get_content_length();

		if (m_content_length == 0) {
			on_request(m_request, boost::asio::buffer("", 0));
		} else {
			m_data.reserve(m_content_length);
		}
	}

	void on_data(const boost::asio::const_buffer &buffer)
	{
		auto begin = boost::asio::buffer_cast<const char *>(buffer);
		auto size = boost::asio::buffer_size(buffer);
		m_data.insert(m_data.end(), begin, begin + size);

		if (m_data.size() == m_content_length) {
			on_request(m_request, boost::asio::buffer(m_data.c_str(), m_data.size()));
		} else if (m_data.size() > m_content_length) {
			this->get_reply()->send_error(swarm::network_reply::bad_request);
		}
	}

	swarm::network_request m_request;
	std::string m_data;
	size_t m_content_length;
};

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_STREAM_HPP
