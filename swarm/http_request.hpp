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

#ifndef IOREMAP_SWARM_HTTP_REQUEST_HPP
#define IOREMAP_SWARM_HTTP_REQUEST_HPP

#include "url.hpp"
#include "http_headers.hpp"

namespace ioremap {
namespace swarm {

class network_request_data;

class http_request
{
public:
	http_request();
	http_request(const boost::none_t &);
	http_request(http_request &&other);
	http_request(const http_request &other);
	~http_request();

	http_request &operator =(http_request &&other);
	http_request &operator =(const http_request &other);

	// Request URL
	const swarm::url &url() const;
	void set_url(const swarm::url &url);
	void set_url(const std::string &url);

	// Checks by Connection header and HTTP version if connection is Keep-Alive
	bool is_keep_alive() const;

	// HTTP headers
	http_headers &headers();
	const http_headers &headers() const;

	// TheVoid specific arguments
	void set_http_version(int major_version, int minor_version);
	int http_major_version() const;
	int http_minor_version() const;

	void set_method(const std::string &method);
	std::string method() const;

private:
	std::unique_ptr<network_request_data> m_data;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_HTTP_REQUEST_HPP
