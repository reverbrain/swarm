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

#include "networkrequest.h"
#include <algorithm>

namespace ioremap {
namespace swarm {

#define CONNECTION_HEADER "Connection"
#define CONNECTION_HEADER_KEEP_ALIVE "Keep-Alive"

static bool are_case_insensitive_equal(const std::string &first, const char *second, const size_t second_size)
{
    if (first.size() != second_size)
        return false;

    for (size_t i = 0; i < second_size; ++i) {
        if (toupper(first[i]) != toupper(second[i]))
            return false;
    }

    return true;
}

class network_request_data
{
public:
    network_request_data()
        : follow_location(false), timeout(30000),
          major_version(1), minor_version(1)
    {
    }
    network_request_data(const network_request_data &o) = default;

    swarm::url url;
    bool follow_location;
    long timeout;
    http_headers headers;
    int major_version;
    int minor_version;
    std::string method;
};

http_request::http_request() : m_data(new network_request_data)
{
}

http_request::http_request(http_request &&other) : m_data(new network_request_data)
{
	using std::swap;
	swap(m_data, other.m_data);
}

http_request::http_request(const http_request &other) : m_data(new network_request_data(*other.m_data))
{
}

http_request::~http_request()
{
}

http_request &http_request::operator =(http_request &&other)
{
	using std::swap;
	swap(m_data, other.m_data);

	return *this;
}

http_request &http_request::operator =(const http_request &other)
{
	using std::swap;
	http_request tmp(other);
	swap(m_data, tmp.m_data);

	return *this;
}

const swarm::url &http_request::url() const
{
    return m_data->url;
}

void http_request::set_url(const swarm::url &url)
{
	m_data->url = url;
}

void http_request::set_url(const std::string &url)
{
	m_data->url = std::move(swarm::url(url));
}

bool http_request::follow_location() const
{
    return m_data->follow_location;
}

void http_request::set_follow_location(bool follow_location)
{
    m_data->follow_location = follow_location;
}

long http_request::timeout() const
{
    return m_data->timeout;
}

void http_request::set_timeout(long timeout)
{
	m_data->timeout = timeout;
}

http_headers &http_request::headers()
{
	return m_data->headers;
}

const http_headers &http_request::headers() const
{
	return m_data->headers;
}

void http_request::set_http_version(int major_version, int minor_version)
{
    m_data->major_version = major_version;
    m_data->minor_version = minor_version;
}

int http_request::http_major_version() const
{
    return m_data->major_version;
}

int http_request::http_minor_version() const
{
    return m_data->minor_version;
}

void http_request::set_method(const std::string &method)
{
    m_data->method = method;
}

std::string http_request::method() const
{
    return m_data->method;
}

bool http_request::is_keep_alive() const
{
	if (auto header = headers().get(CONNECTION_HEADER)) {
		return are_case_insensitive_equal(*header,
			CONNECTION_HEADER_KEEP_ALIVE, sizeof(CONNECTION_HEADER_KEEP_ALIVE) - 1);
	}

	return http_major_version() == 1 && http_minor_version() >= 1;
}

class network_reply_data
{
public:
    network_reply_data()
        : error(0), code(0)
    {
    }
    network_reply_data(const network_reply_data &o) = default;

    http_request request;
    int error;

    int code;
    boost::optional<std::string> reason;
    swarm::url url;
    http_headers headers;
    std::string data;
};

http_response::http_response() : m_data(new network_reply_data)
{
}

http_response::http_response(http_response &&other)
{
	using std::swap;
	swap(m_data, other.m_data);
}

http_response::http_response(const http_response &other) : m_data(new network_reply_data(*other.m_data))
{
}

http_response::~http_response()
{
}

http_response &http_response::operator =(http_response &&other)
{
	using std::swap;
	swap(m_data, other.m_data);
	return *this;
}

http_response &http_response::operator =(const http_response &other)
{
	using std::swap;
	http_response tmp(other);
	swap(m_data, tmp.m_data);
	return *this;
}

const http_request &http_response::request() const
{
    return m_data->request;
}

void http_response::set_request(const http_request &request)
{
    m_data->request = request;
}

int http_response::code() const
{
    return m_data->code;
}

void http_response::set_code(int code)
{
    m_data->code = code;
}

int http_response::error() const
{
    return m_data->error;
}

void http_response::set_error(int error)
{
	m_data->error = error;
}

http_headers &http_response::headers()
{
	return m_data->headers;
}

const http_headers &http_response::headers() const
{
	return m_data->headers;
}

void http_response::set_headers(const http_headers &headers)
{
	m_data->headers = headers;
}

void http_response::set_headers(http_headers &&headers)
{
	m_data->headers = headers;
}

const url &http_response::url() const
{
    return m_data->url;
}

void http_response::set_url(const swarm::url &url)
{
	m_data->url = url;
}

void http_response::set_url(const std::string &url)
{
	m_data->url = std::move(swarm::url(url));
}

const std::string &http_response::data() const
{
    return m_data->data;
}

void http_response::set_data(const std::string &data)
{
    m_data->data = data;
}

}
}
