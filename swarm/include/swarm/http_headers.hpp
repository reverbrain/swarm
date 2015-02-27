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

#ifndef IOREMAP_SWARM_HTTP_HEADERS_HPP
#define IOREMAP_SWARM_HTTP_HEADERS_HPP

#include <boost/optional.hpp>
#include <boost/range.hpp>
#include <vector>
#include <string>
#include <memory>

namespace ioremap {
namespace swarm {

class http_headers_private;

typedef std::pair<std::string, std::string> headers_entry;

/*!
 * \brief The http_headers class is convient class for providing access to http request and reply headers.
 *
 * \sa http_request
 * \sa http_response
 */
class http_headers
{
public:
	/*!
	 * \brief Constructs empty headers list.
	 */
	http_headers();
	/*!
	 * \brief Constructs http_headers by moving list from \a headers.
	 */
	http_headers(std::vector<headers_entry> &&headers);
	/*!
	 * \brief Constructs http_headers by copying list from \a headers.
	 */
	http_headers(const std::vector<headers_entry> &headers);
	/*!
	 * \brief Constructs http_headers by moving list from \a other.
	 */
	http_headers(http_headers &&other);
	/*!
	 * \brief Constructs http_headers by copying list from \a other.
	 */
	http_headers(const http_headers &other);
	/*!
	 * \brief Destroys the list.
	 */
	~http_headers();

	/*!
	 * \brief Moves list from \a other.
	 */
	http_headers &operator =(http_headers &&other);
	/*!
	 * \brief copies list from \a other.
	 */
	http_headers &operator =(const http_headers &other);

	/*!
	 * \brief Returnes list of all headers.
	 */
	const std::vector<headers_entry> &all() const;
	/*!
	 * \overload
	 */
	std::vector<headers_entry> &all();

	/*!
	 * \brief Returnes count of elements in the list.
	 */
	size_t count() const;

	/*!
	 * \brief Returnes true if there is a header with \a name.
	 */
	bool has(const std::string &name) const;

	/*!
	 * \brief Returnes value of header by \a name.
	 */
	boost::optional<std::string> get(const std::string &name) const;
	/*!
	 * \overload
	 */
	boost::optional<std::string> get(const char *name) const;

	/*!
	 * \brief Removes all headers with \a name.
	 *
	 * Returnes number of removed elements.
	 */
	size_t remove(const std::string &name);
	/*!
	 * \overload
	 */
	size_t remove(const char *name);
	/*!
	 * \overload
	 */
	void remove(size_t index);
	/*!
	 * \brief Removes first header with \a name.
	 *
	 * Returnes true if header was removed.
	 */
	bool remove_first(const std::string &name);
	/*!
	 * \overload
	 */
	bool remove_first(const char *name);
	/*!
	 * \brief Removes last header with \a name.
	 *
	 * Returnes true if header was removed.
	 */
	bool remove_last(const std::string &name);
	/*!
	 * \overload
	 */
	bool remove_last(const char *name);

	/*!
	 * \brief Clears list of headers.
	 */
	void clear();

	void assign(std::vector<headers_entry> &&headers);
	void assign(std::initializer_list<headers_entry> headers);
	template <typename Range>
	void assign(const Range &range, typename std::enable_if<!std::is_convertible<Range, std::string>::value>::type * = NULL);
	template <typename Iterator>
	void assign(Iterator begin, Iterator end, typename std::enable_if<!std::is_convertible<Iterator, std::string>::value>::type * = NULL);

	void set(const headers_entry &header);
	void set(headers_entry &&header);
	void set(const std::string &name, const std::string &value);
	void set(const std::string &name, std::initializer_list<std::string> values);
	template <typename Range>
	void set(const std::string &name, const Range &range, typename std::enable_if<!std::is_convertible<Range, std::string>::value>::type * = NULL);
	template <typename Iterator>
	void set(const std::string &name, Iterator begin, Iterator end, typename std::enable_if<!std::is_convertible<Iterator, std::string>::value>::type * = NULL);

	void add(const headers_entry &header);
	void add(headers_entry &&header);
	void add(const std::string &name, const std::string &value);
	void add(const std::string &name, std::initializer_list<std::string> values);
	template <typename Range>
	void add(const std::string &name, const Range &range, typename std::enable_if<!std::is_convertible<Range, std::string>::value>::type * = NULL);
	template <typename Iterator>
	void add(const std::string &name, Iterator begin, Iterator end, typename std::enable_if<!std::is_convertible<Iterator, std::string>::value>::type * = NULL);

	/*!
	 * Returnes time of the last entry update.
	 *
	 * Last update time is passed by Last-Modified HTTP header.
	 * Method supports RFC-1123, RFC-850 and ASCII-time format (similiar to returned by asctime).
	 *
	 * \attention Returned time is number of seconds passed after start of UNIX epoch.
	 *
	 * \sa set_last_modified
	 */
	boost::optional<time_t> last_modified() const;
	/*!
	 * Returnes the value of Last-Modified HTTP header.
	 */
	boost::optional<std::string> last_modified_string() const;
	/*!
	 * Sets the value of Last-Modified HTTP header to \a last_modified.
	 */
	void set_last_modified(const std::string &last_modified);
	/*!
	 * Sets the time of the last entry update to \a last_modified.
	 *
	 * Last update time is passed by Last-Modified HTTP header.
	 * Method supports RFC-1123, RFC-850 and ASCII-time format (similiar to returned by asctime).
	 *
	 * \attention \a Time is number of seconds passed after start of UNIX epoch.
	 *
	 * \sa set_last_modified
	 */
	void set_last_modified(time_t last_modified);

	/*!
	 * Returnes time of the requested last entry update.
	 *
	 * Last update time is passed by If-Modified-Since HTTP header.
	 * Method supports RFC-1123, RFC-850 and ASCII-time format (similiar to returned by asctime).
	 *
	 * If last entry update is older or equal to 'If-Modified-Since' header server should reply by
	 * Not Modified 403 error code.
	 *
	 * \attention Returned time is number of seconds passed after start of UNIX epoch.
	 *
	 * \sa set_if_modified_since
	 */
	boost::optional<time_t> if_modified_since() const;
	/*!
	 * Returnes the value of If-Modified-Since HTTP header.
	 *
	 * \sa set_if_modified_since
	 */
	boost::optional<std::string> if_modified_since_string() const;
	/*!
	 * Sets the value of If-Modified-Since HTTP header to \a time.
	 *
	 * \sa if_modified_since_string
	 */
	void set_if_modified_since(const std::string &time);
	/*!
	 * Sets the time of the requested last entry update to \a time.
	 *
	 * Last update time is passed by If-Modified-Since HTTP header.
	 * Method supports RFC-1123, RFC-850 and ASCII-time format (similiar to returned by asctime).
	 *
	 * If last entry update is older or equal to 'If-Modified-Since' header server should reply by
	 * Not Modified 403 error code.
	 *
	 * \attention \a Time is number of seconds passed after start of UNIX epoch.
	 *
	 * \sa set_if_modified_since
	 */
	void set_if_modified_since(time_t time);

	/*!
	 * \brief Sets the value of Content-Length header to \a length;
	 */
	void set_content_length(size_t length);
	/*!
	 * \brief Returnes the value of Content-Length header.
	 */
	boost::optional<size_t> content_length() const;

	/*!
	 * \brief Sets the value of Content-Type header to \a type;
	 */
	void set_content_type(const std::string &type);
	/*!
	 * \brief Returnes the value of Content-Type header.
	 */
	boost::optional<std::string> content_type() const;

	/*!
	 * \brief Sets the value of Connection header to \a type;
	 *
	 * \sa connection
	 * \sa set_keep_alive
	 */
	void set_connection(const std::string &type);
	/*!
	 * \brief Returnes the value of Connection header.
	 *
	 * \sa set_connection
	 * \sa is_keep_alive
	 */
	boost::optional<std::string> connection() const;

	/*!
	 * \brief Sets the value of Connection header to "Keep-Alive".
	 *
	 * \sa set_connection
	 * \sa is_keep_alive
	 */
	void set_keep_alive();
	/*!
	 * \brief Returnes true if the value of Connection header is "Keep-Alive".
	 *
	 * Is Connection header is not present method returnes invalid boost::optional (so it casts to false).
	 *
	 * \sa set_connection
	 * \sa is_keep_alive
	 */
	boost::optional<bool> is_keep_alive() const;

private:
	std::unique_ptr<http_headers_private> p;
};

template <typename Iterator>
inline void http_headers::assign(Iterator begin, Iterator end, typename std::enable_if<!std::is_convertible<Iterator, std::string>::value>::type *)
{
	std::vector<headers_entry> headers(begin, end);
	assign(std::move(headers));
}

template <typename Range>
inline void http_headers::assign(const Range &range, typename std::enable_if<!std::is_convertible<Range, std::string>::value>::type *)
{
	using boost::begin;
	using boost::end;

	assign(begin(range), end(range));
}

template <typename Iterator>
inline void http_headers::set(const std::string &name, Iterator begin, Iterator end, typename std::enable_if<!std::is_convertible<Iterator, std::string>::value>::type *)
{
	if (begin == end) {
		remove(name);
	} else {
		set(name, static_cast<const std::string &>(*begin));
		std::advance(begin, 1);

		for (; begin != end; ++begin) {
			add(name, *begin);
		}
	}
}

template <typename Range>
inline void http_headers::set(const std::string &name, const Range &range, typename std::enable_if<!std::is_convertible<Range, std::string>::value>::type *)
{
	using boost::begin;
	using boost::end;

	set(name, begin(range), end(range));
}

template <typename Iterator>
inline void http_headers::add(const std::string &name, Iterator begin, Iterator end, typename std::enable_if<!std::is_convertible<Iterator, std::string>::value>::type *)
{
	for (; begin != end; ++begin) {
		add(name, *begin);
	}
}

template <typename Range>
inline void http_headers::add(const std::string &name, const Range &range, typename std::enable_if<!std::is_convertible<Range, std::string>::value>::type *)
{
	using boost::begin;
	using boost::end;

	add(name, begin(range), end(range));
}

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_HTTP_HEADERS_HPP
