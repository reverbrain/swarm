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

#ifndef COCAINE_CRAWLER_NETWORK_URL_H
#define COCAINE_CRAWLER_NETWORK_URL_H

#include <string>
#include <memory>
#include "url_query.hpp"

namespace ioremap {
namespace swarm {

class url_private;

class url
{
public:
	url();
	url(url &&other);
	url(const url &other);
	url(const std::string &url);
	~url();

	url &operator =(url &&other);
	url &operator =(const url &other);
	url &operator =(const std::string &url);

	static url from_user_input(const std::string &url);

	const std::string &original() const;
	std::string to_string() const;

	swarm::url resolved(const swarm::url &relative) const;

	bool is_valid() const;
	bool is_relative() const;

	const std::string &scheme() const;
	void set_scheme(const std::string &scheme);

	const std::string &host() const;
	void set_host(const std::string &host);

	const boost::optional<uint16_t> &port() const;
	void set_port(uint16_t port);

	const std::string &path() const;
	void set_path(const std::string &path);

	const std::vector<std::string> &path_components() const;
	void set_path_components(const std::vector<std::string> &path_components);

	const url_query &query() const;
	url_query &query();
	void set_query(const std::string &query);
	void set_query(const swarm::url_query &query);
	const std::string &raw_query() const;

	const std::string &fragment() const;
	void set_fragment(const std::string &fragment);


private:
	std::unique_ptr<url_private> p;
};

} // namespace crawler
} // namespace cocaine

#endif // COCAINE_CRAWLER_NETWORK_URL_H
