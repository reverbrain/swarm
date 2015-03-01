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

#include "http_response_p.hpp"

#include <algorithm>

namespace ioremap {
namespace swarm {

http_response::http_response() : m_data(new http_response_data)
{
}

http_response::http_response(const boost::none_t &)
{
}

http_response::http_response(http_response &&other)
{
	std::swap(m_data, other.m_data);
}

http_response::http_response(const http_response &other) : m_data(new http_response_data(*other.m_data))
{
}

http_response::~http_response()
{
}

http_response &http_response::operator =(http_response &&other)
{
	std::swap(m_data, other.m_data);
	return *this;
}

http_response &http_response::operator =(const http_response &other)
{
	http_response tmp(other);
	std::swap(m_data, tmp.m_data);
	return *this;
}

int http_response::code() const
{
	return m_data->code;
}

void http_response::set_code(int code)
{
	m_data->code = code;
}

boost::optional<std::string> http_response::reason() const
{
	return m_data->reason;
}

void http_response::set_reason(const std::string &reason)
{
	m_data->reason = reason;
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

} // namespace swarm
} // namespace ioremap
