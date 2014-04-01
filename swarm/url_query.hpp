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

#ifndef IOREMAP_SWARM_NETWORK_QUERY_LIST_H
#define IOREMAP_SWARM_NETWORK_QUERY_LIST_H

#include <memory>
#include <utility>
#include <string>

#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>

namespace ioremap {
namespace swarm {

class network_query_list_private;

/*!
 * \brief The url_query class provides convient interface to work with url query parameters.
 *
 * Url query list is usually an &-separated list of key-value pairs.
 *
 * \sa url
 */
class url_query
{
public:
	/*!
	 * \brief Default constructor. Creates an empty list.
	 */
	url_query();
	/*!
	 * \brief Move constructor from \a other.
	 *
	 * Places \a other values to created url_query. \a other becomes an empty list.
	 */
	url_query(url_query &&other);
	/*!
	 * \brief Copy constructor from \a other.
	 *
	 * Makes deep copy of \a other.
	 */
	url_query(const url_query &other);
	/*!
	 * \brief Contructor from raw string \a query.
	 *
	 * This constructor parses the query.
	 */
	url_query(const std::string &query);

	/*!
	 * Destroyes the query list.
	 */
	~url_query();

	/*!
	 * \brief Move operator from \a other.
	 *
	 * Places \a other values to created url_query. Internally it swapes this list with \a other.
	 */
	url_query &operator =(url_query &&other);
	/*!
	 * \brief Copy operator from \a other.
	 *
	 * Makes deep copy of \a other.
	 */
	url_query &operator =(const url_query &other);

	/*!
	 * \brief Parses \a query and sets internal list of key-pair values from it.
	 */
	void set_query(const std::string &query);
	/*!
	 * \brief Returnes constructed &-separated list of pairs from internal list.
	 */
	std::string to_string() const;

	/*!
	 * \brief Returnes count of pairs in the list.
	 */
	size_t count() const;
	/*!
	 * \brief Returnes key-value pair at \a index in the list.
	 */
	const std::pair<std::string, std::string> &item(size_t index) const;
	/*!
	 * \brief Appends item with \a key and \a value to the list.
	 */
	void add_item(const std::string &key, const std::string &value);
	/*!
	 * \brief Removes item by \a index from the list.
	 */
	void remove_item(size_t index);

	/*!
	 * \brief Returnes true if item with \a key is stored in the list.
	 */
	bool has_item(const std::string &key) const;
	/*!
	 * \brief Returnes item stored by \a key.
	 *
	 * If there is no such item invalid boost::optional is returned.
	 */
	boost::optional<std::string> item_value(const std::string &key) const;
	/*!
	 * \overload
	 */
	boost::optional<std::string> item_value(const char *key) const;

	/*!
	 * \overload
	 *
	 * In opposite to 1-argument item_value if item by \a key does not exists \a default_value is returned.
	 */
	template <typename T>
	T item_value(const std::string &key, const T &default_value) const
	{
		if (auto value = item_value(key))
			return boost::lexical_cast<T>(*value);
		return default_value;
	}

	/*!
	 * \overload
	 */
	template <typename T>
	T item_value(const char *key, const T &default_value) const
	{
		if (auto value = item_value(key))
			return boost::lexical_cast<T>(value);
		return default_value;
	}

private:
	std::unique_ptr<network_query_list_private> p;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_NETWORK_QUERY_LIST_H
