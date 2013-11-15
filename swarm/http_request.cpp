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

#include "http_request.hpp"

namespace ioremap {
namespace swarm {

class network_request_data
{
public:
	network_request_data() : major_version(1), minor_version(1)
	{
	}

	swarm::url url;
	http_headers headers;
	int major_version;
	int minor_version;
	std::string method;
};

http_request::http_request() : m_data(new network_request_data)
{
}

http_request::http_request(const boost::none_t &)
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
	if (auto keep_alive = headers().is_keep_alive()) {
		return *keep_alive;
	}

	return http_major_version() == 1 && http_minor_version() >= 1;
}

} // namespace swarm
} // namespace ioremap
