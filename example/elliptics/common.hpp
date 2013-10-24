/*
 * 2013+ Copyright (c) Ruslan Nigatullin <euroelessar@yandex.ru>
 * 2013+ Copyright (c) Evgeniiy Polyakov <zbr@ioremap.net>
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

#ifndef __IOREMAP_THEVOID_ELLIPTICS_COMMON_HPP
#define __IOREMAP_THEVOID_ELLIPTICS_COMMON_HPP

#include <thevoid/server.hpp>

namespace ioremap { namespace thevoid { namespace elliptics { namespace common {

template <typename T>
struct on_ping : public simple_request_stream<T>, public std::enable_shared_from_this<on_ping<T>> {
	virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) {
		(void) buffer;
		(void) req;

		this->send_reply(swarm::http_response::ok);
	}
};

template <typename T>
struct on_echo : public simple_request_stream<T>, public std::enable_shared_from_this<on_echo<T>> {
	virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) {
		auto data = boost::asio::buffer_cast<const char*>(buffer);
		auto size = boost::asio::buffer_size(buffer);

		swarm::http_response reply;
		reply.set_code(swarm::http_response::ok);
		reply.set_headers(req.headers());
		reply.headers().set_content_length(size);

		this->send_reply(std::move(reply), std::string(data, size));
	}
};

}}}} /* namespace ioremap::thevoid::elliptics::common */

#endif /*__IOREMAP_THEVOID_ELLIPTICS_COMMON_HPP */
