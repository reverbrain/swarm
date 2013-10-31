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

#include "url.hpp"
#include <uriparser/Uri.h>
#include <stdexcept>
#include <iostream>
#include <boost/lexical_cast.hpp>

namespace ioremap {
namespace swarm {

class network_url_cleaner
{
public:
	network_url_cleaner() : uri(NULL) {}
	~network_url_cleaner() { reset(NULL); }
	void reset(UriUriA *new_uri) { if (uri) uriFreeUriMembersA(uri); uri = new_uri; }

private:
	UriUriA *uri;
};

class url_private
{
public:
	enum state_flags {
		invalid         = 0x00,
		parsed          = 0x01,
		invalid_original= 0x02,
		has_original    = 0x04,
		has_changes     = 0x08,
		query_parsed    = 0x10
	};

	url_private() : state(invalid)
	{
	}

	url_private(const url_private &other) = default;

	void ensure_data() const;
	void set_uri(const UriUriA &uri);
	void start_modifications()
	{
		if (state & has_changes) {
			return;
		}
		ensure_data();
		state = parsed | has_changes | (state & query_parsed);
		original = std::string();
	}

	mutable std::string scheme;
	mutable std::string user_name;
	mutable std::string password;
	mutable std::string host;
	mutable std::string path;
	mutable std::string raw_query;
	mutable url_query query;
	mutable std::string fragment;

	std::string original;

	mutable int state;
	mutable boost::optional<uint16_t> port;
};


static bool is_hex(char ch)
{
	return ((ch >= '0' && ch <= '9')
		|| (ch >= 'a' && ch <= 'f')
		|| (ch >= 'A' && ch <= 'F'));
}

static char to_hex(int value)
{
	static const char hex[] = "0123456789abcdef";
	return hex[value];
}

static std::string encode_url(const std::string &url)
{
	std::string tmp;
	tmp.reserve(url.size() * 3);

	// replace stray % by %25
	for (size_t i = 0; i < url.size(); ++i) {
		const char ch = url[i];
		if (ch == '%') {
			if (i + 2 >= url.size() || !is_hex(url[i + 1]) || !is_hex(url[i + 2])) {
				tmp.append("%25");
				continue;
			}
		}
		tmp.push_back(ch);
	}

	size_t hostStart = url.find("//");
	size_t hostEnd = std::string::npos;
	if (hostStart != std::string::npos) {
		// Has host part, find delimiter
		hostStart += 2; // skip "//"
		hostEnd = url.find('/', hostStart);
		if (hostEnd == std::string::npos)
			hostEnd = tmp.find('#', hostStart);
		if (hostEnd == std::string::npos)
			hostEnd = tmp.find('?');
		if (hostEnd == std::string::npos)
			hostEnd = tmp.size() - 1;
	}

	// Reserved and unreserved characters are fine
	//         unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
	//         reserved      = gen-delims / sub-delims
	//         gen-delims    = ":" / "/" / "?" / "#" / "[" / "]" / "@"
	//         sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
	//                         / "*" / "+" / "," / ";" / "="
	// Replace everything else with percent encoding
	static const char doEncode[] = " \"<>[\\]^`{|}";
	static const char doEncodeHost[] = " \"<>\\^`{|}";
	for (size_t i = 0; i < tmp.size(); ++i) {
		unsigned char ch = static_cast<unsigned char>(tmp[i]);
		if (ch < 32 || ch > 127 ||
				strchr(hostStart <= i && i <= hostEnd ? doEncodeHost : doEncode, ch)) {
			char buf[4];
			buf[0] = '%';
			buf[1] = to_hex(ch >> 4);
			buf[2] = to_hex(ch & 0xf);
			buf[3] = '\0';
			tmp.replace(i, 1, buf);
			i += 2;
		}
	}
	return tmp;
}

static std::string to_string(const UriTextRangeA &range)
{
	if (!range.first || !range.afterLast)
		return std::string();
	return std::string(range.first, range.afterLast);
}

void url_private::ensure_data() const
{
	if (state & parsed)
		return;

	UriUriA uri;
	UriParserStateA parser_state;
	parser_state.uri = &uri;
	network_url_cleaner cleaner;

	if (uriParseUriA(&parser_state, original.c_str()) != URI_SUCCESS) {
		state |= (parsed | invalid_original);
		return;
	}

	cleaner.reset(&uri);

	const unsigned int dirtyParts = uriNormalizeSyntaxMaskRequiredA(&uri);
	if (uriNormalizeSyntaxExA(&uri, dirtyParts) != URI_SUCCESS) {
		state |= (parsed | invalid_original);
		return;
	}

	// ensure_data is used for lazy initialization
	const_cast<url_private *>(this)->set_uri(uri);
}

void url_private::set_uri(const UriUriA &uri)
{
	try {
		std::string port_text = to_string(uri.portText);
		if (!port_text.empty())
			port = boost::lexical_cast<int>(port_text);
		else
			port = boost::none;
	} catch (...) {
		port = boost::none;
		state |= (parsed | invalid_original);
		return;
	}

	path = std::string();

	if (uri.absolutePath) {
		path += "/";
	}

	for (auto it = uri.pathHead; it; it = it->next) {
		if (it != uri.pathHead)
			path += "/";
		path += to_string(it->text);
	}

	raw_query = to_string(uri.query);
	host = to_string(uri.hostText);
	scheme = to_string(uri.scheme);
	fragment = to_string(uri.fragment);

	state |= parsed;
}

url::url() : p(new url_private)
{
}

url::url(url &&other)
{
	using std::swap;
	swap(p, other.p);
}

url::url(const url &other) : p(new url_private(*other.p))
{
}

url::url(const std::string &url) : p(new url_private)
{
	p->original = url;
	p->state = url_private::has_original;
}

url::~url()
{
}

url &url::operator =(url &&other)
{
	using std::swap;
	swap(p, other.p);
	return *this;
}

url &url::operator =(const url &other)
{
	url tmp(other);
	swap(p, tmp.p);
	return *this;
}

url &url::operator =(const std::string &url)
{
	swarm::url tmp(url);
	*this = std::move(tmp);
	return *this;
}

url url::from_user_input(const std::string &url)
{
	return std::move(swarm::url(encode_url(url)));
}

const std::string &url::original() const
{
	return p->original;
}

static UriTextRangeA to_range(const std::string &str)
{
	UriTextRangeA range = {
		str.empty() ? NULL : str.c_str(),
		str.empty() ? NULL : (str.c_str() + str.size())
	};
	return range;
}

class uri_generator
{
public:
	explicit uri_generator(const swarm::url_private &url)
	{
		memset(&m_uri, 0, sizeof(m_uri));

		if (url.port) {
			m_port = boost::lexical_cast<std::string>(*url.port);
		}

		if (url.state & url_private::query_parsed) {
			m_query = url.query.to_string();
		} else {
			m_query = url.raw_query;
		}

		m_uri.scheme = to_range(url.scheme);
		m_uri.hostText = to_range(url.host);
		m_uri.portText = to_range(m_port);
		m_uri.query = to_range(m_query);
		m_uri.fragment = to_range(url.fragment);

		size_t start;
		if (url.path.compare(0, 1, "/", 1) == 0) {
			start = 1;
			m_uri.absolutePath = true;
		} else {
			start = 0;
			m_uri.absolutePath = false;
		}

		if (url.path.size() != start) {
			while (true) {
				size_t next = url.path.find('/', start);

				UriPathSegmentA segment = {
					{
						url.path.c_str() + start,
						url.path.c_str() + (next == std::string::npos ? url.path.size() : next)
					},
					NULL,
					NULL
				};
				m_path_segments.push_back(segment);

				if (next == std::string::npos)
					break;

				start = next + 1;
			}

			for (size_t i = 1; i < m_path_segments.size(); ++i) {
				m_path_segments[i - 1].next = &m_path_segments[i];
			}

			m_uri.pathHead = &m_path_segments[0];
		}
	}

	uri_generator(const uri_generator &other) = delete;
	uri_generator &operator =(const uri_generator &other) = delete;

	const UriUriA *uri() const
	{
		return &m_uri;
	}

private:
	UriUriA m_uri;
	std::string m_query;
	std::string m_port;
	std::vector<UriPathSegmentA> m_path_segments;
};

std::string url::to_string() const
{
	if (!is_valid()) {
		return std::string();
	}

	uri_generator uri(*p);

	int chars_required = 0;

	int err = uriToStringCharsRequiredA(uri.uri(), &chars_required);
	if (err) {
		return std::string();
	}

	std::string result;
	result.resize(chars_required + 1);
	int result_size = 0;

	err = uriToStringA(&result[0], uri.uri(), result.size(), &result_size);
	if (err) {
		return std::string();
	}

	result.resize(result_size - 1);

	return result;
}

url url::resolved(const url &relative) const
{
	p->ensure_data();
	relative.p->ensure_data();

	uri_generator absolute_uri(*p);
	uri_generator relative_uri(*relative.p);
	UriUriA result_uri;

	if (uriAddBaseUriA(&result_uri, relative_uri.uri(), absolute_uri.uri()) != URI_SUCCESS) {
		return std::move(url());
	}

	network_url_cleaner cleaner;
	cleaner.reset(&result_uri);

	url result;
	result.p->set_uri(result_uri);

	return std::move(result);
}

bool url::is_valid() const
{
	p->ensure_data();
	return !((p->state & url_private::invalid_original) || (p->state == url_private::invalid));
}

bool url::is_relative() const
{
	p->ensure_data();

	std::cout << "valid: " << is_valid() << ", path: " << path() << ", compare: " << (path().compare(0, 1, "/", 1) != 0) << std::endl;

	return is_valid() && path().compare(0, 1, "/", 1) != 0;
}

const std::string &url::scheme() const
{
	p->ensure_data();
	return p->scheme;
}

void url::set_scheme(const std::string &scheme)
{
	p->start_modifications();
	p->scheme = scheme;
}

const std::string &url::host() const
{
	p->ensure_data();
	return p->host;
}

void url::set_host(const std::string &host)
{
	p->start_modifications();
	p->host = host;
}

const boost::optional<uint16_t> &url::port() const
{
	p->ensure_data();
	return p->port;
}

void url::set_port(uint16_t port)
{
	p->start_modifications();
	p->port = port;
}

const std::string &url::path() const
{
	p->ensure_data();
	return p->path;
}

void url::set_path(const std::string &path)
{
	p->start_modifications();
	p->path = path;
}

const url_query &url::query() const
{
	if (is_valid() && !(p->state & url_private::query_parsed)) {
		p->query = std::move(url_query(p->raw_query));
		p->state |= url_private::query_parsed;
	}

	return p->query;
}

url_query &url::query()
{
	if (is_valid() && !(p->state & url_private::query_parsed)) {
		p->query = std::move(url_query(p->raw_query));
		p->state |= url_private::query_parsed;
	}

	p->start_modifications();

	return p->query;
}

void url::set_query(const std::string &query)
{
	p->start_modifications();
	p->query = std::move(url_query());
	p->raw_query = query;
}

void url::set_query(const url_query &query)
{
	p->start_modifications();
	p->state |= url_private::query_parsed;
	p->raw_query = std::string();
	p->query = query;
}

const std::string &url::raw_query() const
{
	p->ensure_data();
	return p->raw_query;
}

const std::string &url::fragment() const
{
	p->ensure_data();
	return p->fragment;
}

void url::set_fragment(const std::string &fragment)
{
	p->start_modifications();
	p->fragment = fragment;
}

} // namespace crawler
} // namespace cocaine
