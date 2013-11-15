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

namespace detail {
/*
 * Sometimes it's usefull to provide pair of iterators to http_headers::set method
 * to simulate boost::begin/end behaviour.
 */

template <typename Iterator>
Iterator begin(const std::pair<Iterator, Iterator> &range)
{
	return range.first;
}

template <typename Iterator>
Iterator end(const std::pair<Iterator, Iterator> &range)
{
	return range.second;
}
}

class http_headers_private;

typedef std::pair<std::string, std::string> headers_entry;

class http_headers
{
public:
	http_headers();
	http_headers(std::vector<headers_entry> &&headers);
	http_headers(const std::vector<headers_entry> &headers);
	http_headers(http_headers &&other);
	http_headers(const http_headers &other);
	~http_headers();

	http_headers &operator =(http_headers &&other);
	http_headers &operator =(const http_headers &other);

	// List of headers
	const std::vector<headers_entry> &all() const;
	std::vector<headers_entry> &all();

	bool has(const std::string &name) const;

	boost::optional<std::string> get(const std::string &name) const;
	boost::optional<std::string> get(const char *name) const;

	size_t remove(const std::string &name);
	size_t remove(const char *name);
	bool remove_first(const std::string &name);
	bool remove_first(const char *name);
	bool remove_last(const std::string &name);
	bool remove_last(const char *name);

	void set(const std::vector<headers_entry> &headers);
	void set(std::vector<headers_entry> &&headers);
	void set(const headers_entry &header);
	template <typename Range>
	void set(const Range &range);
	void set(const std::string &name, const std::string &value);
	void set(const std::string &name, const char *value);
	template <typename Range>
	void set(const std::string &name, const Range &range);

	void add(const headers_entry &header);
	void add(const std::string &name, const char *value);
	void add(const std::string &name, const std::string &value);
	template <typename Range>
	void add(const std::string &name, const Range &range);

	// Last-Modified, UTC
	boost::optional<time_t> last_modified() const;
	boost::optional<std::string> last_modified_string() const;
	void set_last_modified(const std::string &last_modified);
	void set_last_modified(time_t last_modified);

	// If-Modified-Since, UTC
	boost::optional<time_t> if_modified_since() const;
	boost::optional<std::string> if_modified_since_string() const;
	void set_if_modified_since(const std::string &time);
	void set_if_modified_since(time_t time);

	// Content length
	void set_content_length(size_t length);
	boost::optional<size_t> content_length() const;

	// Content type
	void set_content_type(const std::string &type);
	boost::optional<std::string> content_type() const;

	// Connection
	void set_connection(const std::string &type);
	boost::optional<std::string> connection() const;

	void set_keep_alive();
	boost::optional<bool> is_keep_alive() const;

private:
	std::unique_ptr<http_headers_private> p;
};

template <typename Range>
inline void http_headers::set(const std::string &name, const Range &range)
{
	using std::begin;
	using std::end;
	using ioremap::swarm::detail::begin;
	using ioremap::swarm::detail::end;

	auto begin_it = begin(range);
	auto end_it = end(range);

	if (begin_it == end_it) {
		remove(name);
	} else {
		set(name, static_cast<const std::string &>(*begin_it));
		std::advance(begin_it, 1);

		for (; begin_it != end_it; ++begin_it) {
			add(name, static_cast<const std::string &>(*begin_it));
		}
	}
}

template <typename Range>
inline void http_headers::set(const Range &range)
{
	using std::begin;
	using std::end;
	using ioremap::swarm::detail::begin;
	using ioremap::swarm::detail::end;

	auto begin_it = begin(range);
	auto end_it = end(range);

	std::vector<headers_entry> headers(begin_it, end_it);
	set(std::move(headers));
}

template <typename Range>
inline void http_headers::add(const std::string &name, const Range &range)
{
	using std::begin;
	using std::end;
	using ioremap::swarm::detail::begin;
	using ioremap::swarm::detail::end;

	auto begin_it = begin(range);
	auto end_it = end(range);

	for (; begin_it != end_it; ++begin_it) {
		add(name, static_cast<const std::string &>(*begin_it));
	}
}

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_HTTP_HEADERS_HPP
