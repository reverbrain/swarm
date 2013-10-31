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

	const url_query &query() const;
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
