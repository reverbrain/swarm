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

#ifndef COCAINE_CRAWLER_URL_FINDER_H
#define COCAINE_CRAWLER_URL_FINDER_H

#include <vector>
#include <string>

namespace ioremap {
namespace swarm {

class url_finder
{
public:
	url_finder(const std::string &html);

	const std::vector<std::string> &urls() const;

private:
	void parse() const;

	std::string m_html;
	mutable bool m_parsed;
	mutable std::vector<std::string> m_urls;
};

} // namespace crawler
} // namespace cocaine

#endif // COCAINE_CRAWLER_URL_FINDER_H
