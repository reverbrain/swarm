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

#include "http_headers.hpp"

#include <cstring>

namespace ioremap {
namespace swarm {

#define HTTP_DATE_RFC_1123 "%a, %d %b %Y %H:%M:%S %Z" // Sun, 06 Nov 1994 08:49:37 GMT
#define HTTP_DATE_RFC_850  "%A, %d-%b-%y %H:%M:%S %Z" // Sunday, 06-Nov-94 08:49:37 GMT
#define HTTP_DATE_ASCTIME  "%a %b %e %H:%M:%S %Y"     // Sun Nov  6 08:49:37 1994

#define LAST_MODIFIED_HEADER "Last-Modified"
#define IF_MODIFIED_SINCE_HEADER "If-Modified-Since"
#define CONNECTION_HEADER "Connection"
#define CONNECTION_HEADER_KEEP_ALIVE "Keep-Alive"
#define CONTENT_LENGTH_HEADER "Content-Length"
#define CONTENT_TYPE_HEADER "Content-Type"
#define CONNECTION_HEADER "Connection"
#define CONNECTION_HEADER_KEEP_ALIVE "Keep-Alive"

static bool are_case_insensitive_equal(const std::string &first, const char *second, const size_t second_size)
{
	if (first.size() != second_size)
		return false;

	for (size_t i = 0; i < second_size; ++i) {
		if (toupper(first[i]) != toupper(second[i]))
			return false;
	}

	return true;
}

static std::string convert_to_http_date(time_t time)
{
	struct tm time_data;
	struct tm *tm_result = gmtime_r(&time, &time_data);

	if (!tm_result)
		return std::string();

	char buffer[1024];
	strftime(buffer, sizeof(buffer), HTTP_DATE_RFC_1123, tm_result);
	buffer[sizeof(buffer) - 1] = '\0';

	return buffer;
}

static time_t convert_from_http_date(const std::string &str)
{
	if (str.empty())
		return 0;

	struct tm time_data;
	memset(&time_data, 0, sizeof(struct tm));

	bool success = strptime(str.c_str(), HTTP_DATE_RFC_1123, &time_data)
			|| strptime(str.c_str(), HTTP_DATE_RFC_850, &time_data)
			|| strptime(str.c_str(), HTTP_DATE_ASCTIME, &time_data);

	if (!success)
		return 0;

	return timegm(&time_data);
}

struct is_same_header
{
	const char *name;
	const size_t size;

	bool operator() (const headers_entry &entry) const
	{
		return are_case_insensitive_equal(entry.first, name, size);
	}
};

class http_headers_private
{
public:
	const std::vector<headers_entry> &get_headers() const
	{
		return data;
	}

	std::vector<headers_entry> &get_headers()
	{
		return data;
	}

	void set_headers(const std::vector<headers_entry> &headers)
	{
		data = headers;
	}

	void add_header(const std::string &name, const std::string &value)
	{
		data.emplace_back(name, value);
	}

	struct name_checker
	{
		const char * const name;
		const size_t name_size;

		bool operator() (const headers_entry &value) const
		{
			return are_case_insensitive_equal(value.first, name, name_size);
		}
	};

	void set_header(const std::string &name, const std::string &value)
	{
		name_checker checker = { name.c_str(), name.size() };

		auto position = std::find_if(data.begin(), data.end(), checker) - data.begin();
		auto new_end = std::remove_if(data.begin(), data.end(), checker);
		data.erase(new_end, data.end());
		data.emplace(std::min(data.begin() + position, data.end()), name, value);
	}

	std::vector<headers_entry>::iterator find_header(const char *name, size_t name_size)
	{
		return find_header(data.begin(), data.end(), name, name_size);
	}

	std::vector<headers_entry>::const_iterator find_header(const char *name, size_t name_size) const
	{
		return find_header(data.begin(), data.end(), name, name_size);
	}

	template <size_t N>
	std::vector<headers_entry>::const_iterator find_header(const char (&name)[N]) const
	{
		return find_header(name, N - 1);
	}

	bool has_header(const std::string &name) const
	{
		return find_header(name.c_str(), name.size()) != data.end();
	}

	template <size_t N>
	bool has_header(const char (&name)[N]) const
	{
		return find_header(name, N - 1) != data.end();
	}

	boost::optional<std::string> get_header(const char *name, size_t name_size) const
	{
		auto it = find_header(name, name_size);

		if (it != data.end())
			return it->second;

		return boost::none;
	}

	boost::optional<std::string> get_header(const std::string &name) const
	{
		return get_header(name.c_str(), name.size());
	}

	boost::optional<std::string> get_header(const char *name) const
	{
		return get_header(name, strlen(name));
	}

	template <size_t N>
	boost::optional<std::string> get_header(const char (&name)[N]) const
	{
		return get_header(name, N - 1);
	}

	size_t remove_header(const char *name, size_t length)
	{
		is_same_header pred = { name, length };

		auto it = std::remove_if(data.begin(), data.end(), pred);

		size_t count = data.end() - it;
		data.erase(it, data.end());

		return count;
	}

	bool remove_first_header(const char *name, size_t length)
	{
		is_same_header pred = { name, length };

		auto it = std::find_if(data.begin(), data.end(), pred);

		if (it == data.end()) {
			return false;
		}

		data.erase(it);
		return true;
	}

	bool remove_last_header(const char *name, size_t length)
	{
		is_same_header pred = { name, length };

		auto it = std::find_if(data.rbegin(), data.rend(), pred);

		if (it == data.rend()) {
			return false;
		}

		data.erase(std::prev(it.base()));
		return true;
	}

	boost::optional<std::string> try_header(const char *name, size_t name_size) const
	{
		auto it = find_header(name, name_size);

		if (it != data.end())
			return it->second;

		return boost::none;
	}

	boost::optional<std::string> try_header(const std::string &name) const
	{
		return try_header(name.c_str(), name.size());
	}

	boost::optional<std::string> try_header(const char *name) const
	{
		return try_header(name, strlen(name));
	}

	std::vector<headers_entry>::const_iterator end()
	{
		return data.end();
	}

	std::vector<headers_entry>::const_iterator end() const
	{
		return data.end();
	}

	template <typename T>
	static T find_header(T begin, T end, const char *name, size_t name_size)
	{
		for (; begin != end; ++begin) {
			if (are_case_insensitive_equal(begin->first, name, name_size))
				return begin;
		}

		return end;
	}

	std::vector<headers_entry> data;
};

http_headers::http_headers() : p(new http_headers_private)
{
}

http_headers::http_headers(std::vector<headers_entry> &&headers) : p(new http_headers_private)
{
	p->data = std::move(headers);
}

http_headers::http_headers(const std::vector<headers_entry> &headers) : p(new http_headers_private)
{
	p->data = headers;
}

http_headers::http_headers(http_headers &&other) : p(new http_headers_private)
{
	using std::swap;
	swap(p, other.p);
}

http_headers::http_headers(const http_headers &other) : p(new http_headers_private(*other.p))
{
}

http_headers::~http_headers()
{
}

http_headers &http_headers::operator =(http_headers &&other)
{
	using std::swap;
	swap(p, other.p);

	return *this;
}

http_headers &http_headers::operator =(const http_headers &other)
{
	using std::swap;
	http_headers tmp(other);
	swap(p, tmp.p);

	return *this;
}

const std::vector<headers_entry> &http_headers::all() const
{
	return p->get_headers();
}

std::vector<headers_entry> &http_headers::all()
{
	return p->get_headers();
}

size_t http_headers::count() const
{
	return p->data.size();
}

bool http_headers::has(const std::string &name) const
{
	return p->has_header(name);
}

boost::optional<std::string> http_headers::get(const std::string &name) const
{
	return p->try_header(name);
}

boost::optional<std::string> http_headers::get(const char *name) const
{
	return p->try_header(name);
}

size_t http_headers::remove(const std::string &name)
{
	return p->remove_header(name.c_str(), name.size());
}

size_t http_headers::remove(const char *name)
{
	return p->remove_header(name, strlen(name));
}

void http_headers::remove(size_t index)
{
	p->data.erase(p->data.begin() + index);
}

bool http_headers::remove_first(const std::string &name)
{
	return p->remove_first_header(name.c_str(), name.size());
}

bool http_headers::remove_first(const char *name)
{
	return p->remove_first_header(name, strlen(name));
}

bool http_headers::remove_last(const std::string &name)
{
	return p->remove_last_header(name.c_str(), name.size());
}

bool http_headers::remove_last(const char *name)
{
	return p->remove_last_header(name, strlen(name));

}

void http_headers::clear()
{
	p->data.clear();
}

void http_headers::assign(std::vector<headers_entry> &&headers)
{
	p->data = std::move(headers);
}

void http_headers::assign(std::initializer_list<headers_entry> headers)
{
	p->data = headers;
}

void http_headers::set(const std::string &name, const std::string &value)
{
	p->set_header(name, value);
}

void http_headers::set(const std::string &name, std::initializer_list<std::string> values)
{
	set(name, values.begin(), values.end());
}

void http_headers::add(const headers_entry &header)
{
	p->add_header(header.first, header.second);
}

void http_headers::add(headers_entry &&header)
{
	p->data.emplace_back(std::move(header));
}

void http_headers::add(const std::string &name, const std::string &value)
{
	p->add_header(name, value);
}

void http_headers::add(const std::string &name, std::initializer_list<std::string> values)
{
	add(name, values.begin(), values.end());
}

boost::optional<time_t> http_headers::last_modified() const
{
	if (auto http_date = last_modified_string())
		return convert_from_http_date(*http_date);
	return boost::none;
}

boost::optional<std::string> http_headers::last_modified_string() const
{
	return get(LAST_MODIFIED_HEADER);
}

void http_headers::set_last_modified(const std::string &last_modified)
{
	p->set_header(LAST_MODIFIED_HEADER, last_modified);
}

void http_headers::set_last_modified(time_t last_modified)
{
	set_last_modified(convert_to_http_date(last_modified));
}

boost::optional<time_t> http_headers::if_modified_since() const
{
	if (auto http_date = if_modified_since_string())
		return convert_from_http_date(*http_date);

	return boost::none;
}

boost::optional<std::string> http_headers::if_modified_since_string() const
{
	return get(IF_MODIFIED_SINCE_HEADER);
}

void http_headers::set_if_modified_since(const std::string &time)
{
	p->set_header(IF_MODIFIED_SINCE_HEADER, time);
}

void http_headers::set_if_modified_since(time_t time)
{
	set_if_modified_since(convert_to_http_date(time));
}

void http_headers::set_content_length(size_t length)
{
	char buffer[20];
	snprintf(buffer, sizeof(buffer), "%zu", length);
	p->set_header(CONTENT_LENGTH_HEADER, buffer);
}

boost::optional<size_t> http_headers::content_length() const
{
	if (auto header = p->get_header(CONTENT_LENGTH_HEADER)) {
		return atoll(header->c_str());
	}

	return boost::none;
}

void http_headers::set_content_type(const std::string &type)
{
	p->set_header(CONTENT_TYPE_HEADER, type);
}

boost::optional<std::string> http_headers::content_type() const
{
	return p->get_header(CONTENT_TYPE_HEADER);
}

void http_headers::set_connection(const std::string &type)
{
	p->set_header(CONNECTION_HEADER, type);
}

boost::optional<std::string> http_headers::connection() const
{
	return p->get_header(CONNECTION_HEADER);
}

void http_headers::set_keep_alive()
{
	set_connection(CONNECTION_HEADER_KEEP_ALIVE);
}

boost::optional<bool> http_headers::is_keep_alive() const
{
	if (auto tmp = connection()) {
		return are_case_insensitive_equal(*tmp, CONNECTION_HEADER_KEEP_ALIVE, sizeof(CONNECTION_HEADER_KEEP_ALIVE) - 1);
	}

	return boost::none;
}

} // namespace swarm
} // namespace ioremap
