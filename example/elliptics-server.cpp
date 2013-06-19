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

#include "elliptics-server.hpp"
#include <iostream>

using namespace ioremap::thevoid;

elliptics_server::elliptics_server()
{
}

void elliptics_server::initialize()
{
	on<on_update>("/update");
	on<on_find>("/find");
	on<on_ping>("/ping");
}

void elliptics_server::on_update::on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
    const auto &headers = req.get_headers();
	for (auto it = headers.begin(); it != headers.end(); ++it) {
		std::cerr << "header: " << it->first << " = " << it->second << std::endl;
	}
	std::cerr << "data: size: " << boost::asio::buffer_size(buffer) << std::endl;

	get_reply()->send_error(swarm::network_reply::not_implemented);
}

void elliptics_server::on_update::on_close(const boost::system::error_code &err)
{
	std::cerr << "closed" << std::endl;
}

void elliptics_server::on_find::on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	get_reply()->send_error(swarm::network_reply::not_implemented);
}

void elliptics_server::on_find::on_close(const boost::system::error_code &err)
{
}

void elliptics_server::on_ping::on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	auto conn = get_reply();

	swarm::network_reply reply;
    reply.set_code(swarm::network_reply::ok);
    reply.set_content_length(0);
    reply.set_content_type("text/html");

	conn->send_headers(reply, boost::asio::buffer(""),
			   std::bind(&reply_stream::close, conn, std::placeholders::_1));
}

void elliptics_server::on_ping::on_close(const boost::system::error_code &err)
{
	(void) err;
}


int main(int argc, char **argv)
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " address" << std::endl;
		return 1;
	}

	auto server = create_server<elliptics_server>();
	for (int i = 1; i < argc; ++i)
		server->listen(argv[i]);
	server->run();

	return 0;
}
