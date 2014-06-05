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

#include <memory>
#include <string>
#include <vector>

#include "url_query.hpp"

namespace ioremap {
namespace swarm {

class url_private;

/*!
 * \brief The url class provides convient interface for working with URLs.
 *
 * Url may be constructed by calling methods set_scheme, set_host, set_path, etc.
 */
class url
{
public:
	/*!
	 * \brief Default constructor, constructs invalid url.
	 * After construction url's properties may be filled by proper values.
	 */
	url();
	/*!
	 * \brief Move constructor from \a other url. \a Other url becomes invalid one.
	 */
	url(url &&other);
	/*!
	 * \brief Copy constructor. Makes deep copy of \a other url.
	 */
	url(const url &other);
	/*!
	 * \brief Constructor from string representation of \a url.
	 *
	 * Url performes lazy parsing of a string by request of any method except original.
	 */
	url(const std::string &url);
	/*!
	 * Destroyes the url.
	 */
	~url();

	/*!
	 * \brief Move operator. Internally makes fast swap of urls.
	 */
	url &operator =(url &&other);
	/*!
	 * \brief Assigmnent operator. Makes deep copy of \a other url.
	 */
	url &operator =(const url &other);
	/*!
	 * \brief Assigmnent operator. Assignes \a url as original string.
	 * Parsing of this string is lazy and performed only by request.
	 */
	url &operator =(const std::string &url);

	/*!
	 * \brief Create url from user-typed \a url.
	 * Allows mix of percent encoding and using of unencoded utf-8 characters.
	 *
	 * Use it if you are not sure that url is certainly valid.
	 */
	static url from_user_input(const std::string &url);

	/*!
	 * \brief Returns original string this url was constructed by.
	 */
	const std::string &original() const;
	/*!
	 * \brief Return url encoded by percent encoding.
	 */
	std::string to_string() const;

	/*!
	 * \brief Returns human readable url representation
	 */
	std::string to_human_readable() const;

	/*!
	 * \brief Returns the merge of \a relative url with current one.
	 *
	 * If \a relative is not relative url it's returned directly.
	 */
	swarm::url resolved(const swarm::url &relative) const;

	/*!
	 * \brief Returns true if url is non-empty and valid.
	 */
	bool is_valid() const;
	/*!
	 * \brief Returns true is url is relative (it is valid and starts with '/').
	 */
	bool is_relative() const;

	/*!
	 * \brief Returns scheme of the url if defined and empty string otherwise.
	 */
	const std::string &scheme() const;
	/*!
	 * \brief Set the sceme of the url to \a scheme.
	 */
	void set_scheme(const std::string &scheme);

	/*!
	 * \brief Returns the host of the url if defined and empty string otherwise.
	 */
	const std::string &host() const;
	/*!
	 * \brief Set the host of the url to \a host.
	 */
	void set_host(const std::string &host);

	/*!
	 * \brief Returns port of the url if defined.
	 */
	const boost::optional<uint16_t> &port() const;
	/*!
	 * \brief Set the port of the url to \a port.
	 */
	void set_port(uint16_t port);

	/*!
	 * \brief Returns path of the url if defined and empty string otherwise.
	 */
	const std::string &path() const;
	/*!
	 * \brief Set the path of the url to \a path.
	 */
	void set_path(const std::string &path);


	/*!
	 * \brief Sets path components - array of strings, which after being joined by '/' symbol form full path.
	 */
	const std::vector<std::string> &path_components() const;

	/*!
	 * \brief Returns query of the url.
	 *
	 * \sa url_query
	 */
	const url_query &query() const;
	/*!
	 * \brief Returns query of the url.
	 *
	 * \sa url_query
	 */
	url_query &query();
	/*!
	 * \brief Set the query of the url to \a query.
	 *
	 * \sa url_query
	 */
	void set_query(const std::string &query);
	/*!
	 * \brief Set the query of the url to \a query.
	 *
	 * \sa url_query
	 */
	void set_query(const swarm::url_query &query);
	/*!
	 * \brief Returns original raw query of the url if defined and empty string otherwise.
	 *
	 * \sa url_query
	 */
	const std::string &raw_query() const;

	/*!
	 * \brief Returns fragment of the url if defined and empty string otherwise.
	 */
	const std::string &fragment() const;
	/*!
	 * \brief Set the fragment of the url to \a fragment.
	 */
	void set_fragment(const std::string &fragment);


private:
	std::unique_ptr<url_private> p;
};

} // namespace crawler
} // namespace cocaine

#endif // COCAINE_CRAWLER_NETWORK_URL_H
