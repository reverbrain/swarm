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

#include <cstring>

#include <boost/lexical_cast.hpp>

namespace ioremap {
namespace swarm {

http_response::http_response() : m_data(new http_response_data)
{
}

http_response::http_response(http_response_data &data) : m_data(&data)
{
}

http_response::http_response(const boost::none_t &)
{
}

http_response::http_response(http_response &&other)
{
	using std::swap;
	swap(m_data, other.m_data);
}

http_response::http_response(const http_response &other) : m_data(new http_response_data(*other.m_data))
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

int http_response::code() const
{
	return m_data->code;
}

void http_response::set_code(int code)
{
	m_data->code = code;
	m_data->code_str = boost::lexical_cast<std::string>(code);
}

boost::optional<std::string> http_response::reason() const
{
	return m_data->reason;
}

void http_response::set_reason(const std::string &reason)
{
	m_data->reason = reason;
}

const char* http_response::default_reason(int code) {
	switch (code) {
		case 100: return "Continue";
		case 101: return "Switching Protocols";
		case 102: return "Processing";

		case 200: return "OK";
		case 201: return "Created";
		case 202: return "Accepted";
		case 203: return "Non-Authoritative Information";
		case 204: return "No Content";
		case 205: return "Reset Content";
		case 206: return "Partial Content";
		case 207: return "Multi-Status";
		case 208: return "Already Reported";
		case 209: return "IM Used";

		case 300: return "Multiple Choices";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 304: return "Not Modified";
		case 305: return "Use Proxy";
		case 306: return "Switch Proxy";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";

		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 402: return "Payment Required";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 406: return "Not Acceptable";
		case 407: return "Proxy Authentication Required";
		case 408: return "Request Timeout";
		case 409: return "Conflict";
		case 410: return "Gone";
		case 411: return "Length Required";
		case 412: return "Precondition Failed";
		case 413: return "Request Entity Too Large";
		case 414: return "Request-URI Too Long";
		case 415: return "Unsupported Media Type";
		case 416: return "Requested Range Not Satisfiable";
		case 417: return "Expectation Failed";
		case 418: return "I'm a teapot";
		case 419: return "Authentication Timeout";
		case 422: return "Unprocessable Entity";
		case 423: return "Locked";
		case 424: return "Failed Dependency";
//		case 424: return "Method Failure"; // WebDav
//		case 425: return "Unordered Collection"; // Internet draft
		case 426: return "Upgrade Required";
		case 428: return "Precondition Required";
		case 429: return "Too Many Requests";
		case 431: return "Request Header Fields Too Large";
		case 444: return "No Response";
//		case 451: return "Unavailable For Legal Reasons"; // Internet draft

		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		case 506: return "Variant Also Negotiates";
		case 507: return "Insufficient Storage";
		case 508: return "Loop Detected";
		case 510: return "Not Extended";
		case 511: return "Network Authentication Required";
		case 522: return "Connection timed out";
	}

	return "-";
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

// Remove the null terminating character from string literal
template <size_t N>
static
boost::asio::const_buffer to_buffer(const char (&str)[N]) {
	return boost::asio::buffer(str, N - 1);
}

std::vector<boost::asio::const_buffer> http_response::to_buffers() const {
	static const char name_value_separator[] = { ':', ' ' };
	static const char crlf[] = { '\r', '\n' };

	const auto &headers = this->headers().all();

	std::vector<boost::asio::const_buffer> buffers;
	buffers.reserve(5 + headers.size() * 4 + 2);

	buffers.push_back(to_buffer("HTTP/1.1 "));
	buffers.push_back(boost::asio::buffer(m_data->code_str));
	buffers.push_back(to_buffer(" "));

	if (auto reason_phrase = m_data->reason) {
		buffers.push_back(boost::asio::buffer(*reason_phrase));
	}
	else {
		const char* reason = default_reason(code());
		size_t reason_len = strlen(reason);
		buffers.push_back(boost::asio::buffer(reason, reason_len));
	}

	buffers.push_back(boost::asio::buffer(crlf));

	for (std::size_t i = 0; i < headers.size(); ++i) {
		auto &header = headers[i];
		buffers.push_back(boost::asio::buffer(header.first));
		buffers.push_back(boost::asio::buffer(name_value_separator));
		buffers.push_back(boost::asio::buffer(header.second));
		buffers.push_back(boost::asio::buffer(crlf));
	}

	buffers.push_back(boost::asio::buffer(crlf));

	return buffers;
}

} // namespace swarm
} // namespace ioremap
