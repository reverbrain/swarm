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

#include "stockreplies_p.hpp"

namespace ioremap {
namespace thevoid {

namespace stock_replies {

template <size_t N>
static boost::asio::const_buffer to_buffer(const char (&str)[N])
{
	return boost::asio::buffer(str, N - 1);
}

namespace status_strings {

const char ok[] =
		"HTTP/1.1 200 OK\r\n";
const char created[] =
		"HTTP/1.1 201 Created\r\n";
const char accepted[] =
		"HTTP/1.1 202 Accepted\r\n";
const char no_content[] =
		"HTTP/1.1 204 No Content\r\n";
const char multiple_choices[] =
		"HTTP/1.1 300 Multiple Choices\r\n";
const char moved_permanently[] =
		"HTTP/1.1 301 Moved Permanently\r\n";
const char moved_temporarily[] =
		"HTTP/1.1 302 Moved Temporarily\r\n";
const char not_modified[] =
		"HTTP/1.1 304 Not Modified\r\n";
const char bad_request[] =
		"HTTP/1.1 400 Bad Request\r\n";
const char unauthorized[] =
		"HTTP/1.1 401 Unauthorized\r\n";
const char forbidden[] =
		"HTTP/1.1 403 Forbidden\r\n";
const char not_found[] =
		"HTTP/1.1 404 Not Found\r\n";
const char internal_server_error[] =
		"HTTP/1.1 500 Internal Server Error\r\n";
const char not_implemented[] =
		"HTTP/1.1 501 Not Implemented\r\n";
const char bad_gateway[] =
		"HTTP/1.1 502 Bad Gateway\r\n";
const char service_unavailable[] =
		"HTTP/1.1 503 Service Unavailable\r\n";

boost::asio::const_buffer to_buffer(int status)
{
	using ioremap::thevoid::stock_replies::to_buffer;

	switch (status) {
		case swarm::network_reply::ok:
			return to_buffer(ok);
		case swarm::network_reply::created:
			return to_buffer(created);
		case swarm::network_reply::accepted:
			return to_buffer(accepted);
		case swarm::network_reply::no_content:
			return to_buffer(no_content);
		case swarm::network_reply::multiple_choices:
			return to_buffer(multiple_choices);
		case swarm::network_reply::moved_permanently:
			return to_buffer(moved_permanently);
		case swarm::network_reply::moved_temporarily:
			return to_buffer(moved_temporarily);
		case swarm::network_reply::not_modified:
			return to_buffer(not_modified);
		case swarm::network_reply::bad_request:
			return to_buffer(bad_request);
		case swarm::network_reply::unauthorized:
			return to_buffer(unauthorized);
		case swarm::network_reply::forbidden:
			return to_buffer(forbidden);
		case swarm::network_reply::not_found:
			return to_buffer(not_found);
		case swarm::network_reply::internal_server_error:
			return to_buffer(internal_server_error);
		case swarm::network_reply::not_implemented:
			return to_buffer(not_implemented);
		case swarm::network_reply::bad_gateway:
			return to_buffer(bad_gateway);
		case swarm::network_reply::service_unavailable:
			return to_buffer(service_unavailable);
		default:
			return to_buffer(internal_server_error);
	}
}

} // namespace status_strings

namespace misc_strings {

const char name_value_separator[] = { ':', ' ' };
const char crlf[] = { '\r', '\n' };

} // namespace misc_strings

namespace default_content {

const char ok[] = "";
const char created[] =
		"<html>"
		"<head><title>Created</title></head>"
		"<body><h1>201 Created</h1></body>"
		"</html>";
const char accepted[] =
		"<html>"
		"<head><title>Accepted</title></head>"
		"<body><h1>202 Accepted</h1></body>"
		"</html>";
const char no_content[] =
		"<html>"
		"<head><title>No Content</title></head>"
		"<body><h1>204 Content</h1></body>"
		"</html>";
const char multiple_choices[] =
		"<html>"
		"<head><title>Multiple Choices</title></head>"
		"<body><h1>300 Multiple Choices</h1></body>"
		"</html>";
const char moved_permanently[] =
		"<html>"
		"<head><title>Moved Permanently</title></head>"
		"<body><h1>301 Moved Permanently</h1></body>"
		"</html>";
const char moved_temporarily[] =
		"<html>"
		"<head><title>Moved Temporarily</title></head>"
		"<body><h1>302 Moved Temporarily</h1></body>"
		"</html>";
const char not_modified[] =
		"<html>"
		"<head><title>Not Modified</title></head>"
		"<body><h1>304 Not Modified</h1></body>"
		"</html>";
const char bad_request[] =
		"<html>"
		"<head><title>Bad Request</title></head>"
		"<body><h1>400 Bad Request</h1></body>"
		"</html>";
const char unauthorized[] =
		"<html>"
		"<head><title>Unauthorized</title></head>"
		"<body><h1>401 Unauthorized</h1></body>"
		"</html>";
const char forbidden[] =
		"<html>"
		"<head><title>Forbidden</title></head>"
		"<body><h1>403 Forbidden</h1></body>"
		"</html>";
const char not_found[] =
		"<html>"
		"<head><title>Not Found</title></head>"
		"<body><h1>404 Not Found</h1></body>"
		"</html>";
const char internal_server_error[] =
		"<html>"
		"<head><title>Internal Server Error</title></head>"
		"<body><h1>500 Internal Server Error</h1></body>"
		"</html>";
const char not_implemented[] =
		"<html>"
		"<head><title>Not Implemented</title></head>"
		"<body><h1>501 Not Implemented</h1></body>"
		"</html>";
const char bad_gateway[] =
		"<html>"
		"<head><title>Bad Gateway</title></head>"
		"<body><h1>502 Bad Gateway</h1></body>"
		"</html>";
const char service_unavailable[] =
		"<html>"
		"<head><title>Service Unavailable</title></head>"
		"<body><h1>503 Service Unavailable</h1></body>"
		"</html>";

static boost::asio::const_buffer to_buffer(int status)
{
    using ioremap::thevoid::stock_replies::to_buffer;

	switch (status) {
		case swarm::network_reply::ok:
			return to_buffer(ok);
		case swarm::network_reply::created:
			return to_buffer(created);
		case swarm::network_reply::accepted:
			return to_buffer(accepted);
		case swarm::network_reply::no_content:
			return to_buffer(no_content);
		case swarm::network_reply::multiple_choices:
			return to_buffer(multiple_choices);
		case swarm::network_reply::moved_permanently:
			return to_buffer(moved_permanently);
		case swarm::network_reply::moved_temporarily:
			return to_buffer(moved_temporarily);
		case swarm::network_reply::not_modified:
			return to_buffer(not_modified);
		case swarm::network_reply::bad_request:
			return to_buffer(bad_request);
		case swarm::network_reply::unauthorized:
			return to_buffer(unauthorized);
		case swarm::network_reply::forbidden:
			return to_buffer(forbidden);
		case swarm::network_reply::not_found:
			return to_buffer(not_found);
		case swarm::network_reply::internal_server_error:
			return to_buffer(internal_server_error);
		case swarm::network_reply::not_implemented:
			return to_buffer(not_implemented);
		case swarm::network_reply::bad_gateway:
			return to_buffer(bad_gateway);
		case swarm::network_reply::service_unavailable:
			return to_buffer(service_unavailable);
		default:
			return to_buffer(internal_server_error);
	}
}

} // default_content

boost::asio::const_buffer status_to_buffer(swarm::network_reply::status_type status)
{
    return status_strings::to_buffer(status);
}

boost::asio::const_buffer status_content(swarm::network_reply::status_type status)
{
    return default_content::to_buffer(status);
}

swarm::network_reply stock_reply(swarm::network_reply::status_type status)
{
	const size_t buffer_size = boost::asio::buffer_size(status_content(status));

    swarm::network_reply reply;
    reply.set_code(status);
    reply.set_content_length(buffer_size);
    reply.set_content_type("text/html");

    return reply;
}

std::vector<boost::asio::const_buffer> to_buffers(const swarm::network_reply &reply, const boost::asio::const_buffer &content)
{
	std::vector<boost::asio::const_buffer> buffers;
	buffers.push_back(status_strings::to_buffer(reply.get_code()));

    const auto &headers = reply.get_headers();
	for (std::size_t i = 0; i < headers.size(); ++i) {
		auto &header = headers[i];
		buffers.push_back(boost::asio::buffer(header.first));
		buffers.push_back(boost::asio::buffer(misc_strings::name_value_separator));
		buffers.push_back(boost::asio::buffer(header.second));
		buffers.push_back(boost::asio::buffer(misc_strings::crlf));
	}
	buffers.push_back(boost::asio::buffer(misc_strings::crlf));
	buffers.push_back(content);
	return buffers;
}

} // namespace stock_replies

} } // namespace ioremap::thevoid
