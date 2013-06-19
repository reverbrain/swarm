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

#include <thevoid/server.hpp>

namespace ioremap {
namespace thevoid {

class elliptics_server : public server<elliptics_server>
{
public:
	elliptics_server();

	void initialize() /*override*/;

	struct on_update : public simple_request_stream<elliptics_server>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_close(const boost::system::error_code &err) /*override*/;
	};

	struct on_find : public simple_request_stream<elliptics_server>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_close(const boost::system::error_code &err) /*override*/;
	};

	struct on_ping : public simple_request_stream<elliptics_server>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_close(const boost::system::error_code &err) /*override*/;
	};
};

} } // namespace ioremap::thevoid
