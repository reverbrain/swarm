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

#define CASE_CODE(code, message) case code: return stock_replies::to_buffer("HTTP/1.1 " #code " " message "\r\n")

boost::asio::const_buffer to_buffer(int status)
{
	switch (status) {
		CASE_CODE(100, "Continue");
		CASE_CODE(101, "Switching Protocols");
		CASE_CODE(102, "Processing");

		CASE_CODE(200, "OK");
	        CASE_CODE(201, "Created");
		CASE_CODE(202, "Accepted");
		CASE_CODE(203, "Non-Authoritative Information");
		CASE_CODE(204, "No Content");
		CASE_CODE(205, "Reset Content");
		CASE_CODE(206, "Partial Content");
		CASE_CODE(207, "Multi-Status");
		CASE_CODE(208, "Already Reported");
		CASE_CODE(209, "IM Used");

	        CASE_CODE(300, "Multiple Choices");
	        CASE_CODE(301, "Moved Permanently");
	        CASE_CODE(302, "Moved Temporarily");
		CASE_CODE(304, "Not Modified");
		CASE_CODE(305, "Use Proxy");
		CASE_CODE(306, "Switch Proxy");
		CASE_CODE(307, "Temporary Redirect");
		CASE_CODE(308, "Permanent Redirect");

		CASE_CODE(400, "Bad Request");
		CASE_CODE(401, "Unauthorized");
		CASE_CODE(402, "Payment Required");
		CASE_CODE(403, "Forbidden");
		CASE_CODE(404, "Not Found");
		CASE_CODE(405, "Method Not Allowed");
		CASE_CODE(406, "Not Acceptable");
		CASE_CODE(407, "Proxy Authentication Required");
		CASE_CODE(408, "Request Timeout");
		CASE_CODE(409, "Conflict");
		CASE_CODE(410, "Gone");
		CASE_CODE(411, "Length Required");
		CASE_CODE(412, "Precondition Failed");
		CASE_CODE(413, "Request Entity Too Large");
		CASE_CODE(414, "Request-URI Too Long");
		CASE_CODE(415, "Unsupported Media Type");
		CASE_CODE(416, "Requested Range Not Satisfiable");
		CASE_CODE(417, "Expectation Failed");
		CASE_CODE(418, "I'm a teapot");
		CASE_CODE(419, "Authentication Timeout");
		CASE_CODE(422, "Unprocessable Entity");
		CASE_CODE(423, "Locked");
		CASE_CODE(424, "Failed Dependency");
//		CASE_CODE(424, "Method Failure"); -- WebDav
//		CASE_CODE(425, "Unordered Collection"); -- Internet draft
		CASE_CODE(426, "Upgrade Required");
		CASE_CODE(428, "Precondition Required");
		CASE_CODE(429, "Too Many Requests");
		CASE_CODE(431, "Request Header Fields Too Large");
		CASE_CODE(444, "No Response");
//		CASE_CODE(451, "Unavailable For Legal Reasons"); -- Internet draft

		default:
		CASE_CODE(500, "Internal Server Error");
		CASE_CODE(501, "Not Implemented");
		CASE_CODE(502, "Bad Gateway");
		CASE_CODE(503, "Service Unavailable");
		CASE_CODE(504, "Gateway Timeout");
		CASE_CODE(505, "HTTP Version Not Supported");
		CASE_CODE(506, "Variant Also Negotiates");
		CASE_CODE(507, "Insufficient Storage");
		CASE_CODE(508, "Loop Detected");
		CASE_CODE(510, "Not Extended");
		CASE_CODE(511, "Network Authentication Required");
		CASE_CODE(522, "Connection timed out");
	}
}

#undef CASE_CODE

} // namespace status_strings

namespace misc_strings {

const char name_value_separator[] = { ':', ' ' };
const char crlf[] = { '\r', '\n' };

} // namespace misc_strings

boost::asio::const_buffer status_to_buffer(http_response::status_type status)
{
    return status_strings::to_buffer(status);
}

http_response stock_reply(http_response::status_type status)
{
	http_response reply;
	reply.set_code(status);
	reply.headers().set_content_length(0);
	return reply;
}

std::vector<boost::asio::const_buffer> to_buffers(const http_response &reply, const boost::asio::const_buffer &content)
{
	const auto &headers = reply.headers().all();

	std::vector<boost::asio::const_buffer> buffers;
	buffers.reserve(1 + headers.size() * 4 + 2);

	buffers.push_back(status_strings::to_buffer(reply.code()));

	for (std::size_t i = 0; i < headers.size(); ++i) {
		auto &header = headers[i];
		buffers.push_back(boost::asio::buffer(header.first));
		buffers.push_back(boost::asio::buffer(misc_strings::name_value_separator));
		buffers.push_back(boost::asio::buffer(header.second));
		buffers.push_back(boost::asio::buffer(misc_strings::crlf));
	}

	buffers.push_back(boost::asio::buffer(misc_strings::crlf));
	buffers.push_back(content);

	return std::move(buffers);
}

static inline void push_back(std::vector<char> &result, const boost::asio::const_buffer &buffer)
{
	const auto data = boost::asio::buffer_cast<const char *>(buffer);
	const auto size = boost::asio::buffer_size(buffer);
	result.insert(result.end(), data, data + size);
}

static inline void push_back(std::vector<char> &result, const std::string &buffer)
{
	result.insert(result.end(), buffer.begin(), buffer.end());
}

template <size_t N>
static inline void push_back(std::vector<char> &result, const char (&buffer)[N])
{
	result.insert(result.end(), buffer, buffer + N);
}

void to_buffers(const http_response &reply, std::vector<char> &buffer)
{
	buffer.reserve(1024);

	push_back(buffer, status_strings::to_buffer(reply.code()));

	const auto &headers = reply.headers().all();
	for (std::size_t i = 0; i < headers.size(); ++i) {
		auto &header = headers[i];
		push_back(buffer, header.first);
		push_back(buffer, misc_strings::name_value_separator);
		push_back(buffer, header.second);
		push_back(buffer, misc_strings::crlf);
	}
	push_back(buffer, misc_strings::crlf);
}

} // namespace stock_replies

} } // namespace ioremap::thevoid
