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

#include "http_request_p.hpp"

#include <algorithm>

namespace ioremap {
namespace swarm {

http_request::http_request() : m_data(new http_request_data)
{
}

http_request::http_request(const boost::none_t &)
{
}

http_request::http_request(http_request &&other)
{
	std::swap(m_data, other.m_data);
}

http_request::http_request(const http_request &other) : m_data(new http_request_data(*other.m_data))
{
}

http_request::~http_request()
{
}

http_request &http_request::operator =(http_request &&other)
{
	std::swap(m_data, other.m_data);

	return *this;
}

http_request &http_request::operator =(const http_request &other)
{
	http_request tmp(other);
	std::swap(m_data, tmp.m_data);

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

void http_request::set_method(const std::string &method)
{
	m_data->method = method;
}

std::string http_request::method() const
{
	return m_data->method;
}

} // namespace swarm
} // namespace ioremap
