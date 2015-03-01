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

#include <swarm/url.hpp>
#include <uriparser/Uri.h>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <punycode.h>
#include <stringprep.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

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
		query_parsed    = 0x10,
		has_human_readable = 0x20
	};

	url_private() : state(invalid)
	{
	}

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
		human_readable = std::string();
	}

	mutable std::string scheme;
	mutable std::string user_name;
	mutable std::string password;
	mutable std::string host;
	mutable std::string path;
	mutable std::vector<std::string> path_components;
	mutable std::string raw_query;
	mutable url_query query;
	mutable std::string fragment;
	mutable std::string human_readable;

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

//! Returns puny-encoded host if it's not rfc-compatible
static std::string encode_host(const std::string &host)
{
	bool host_ok = true;
	// According to RFC-1738 hostname may contain only alpha-numeric characters and dash
	// Dot is separator for different domain names levels
	for (size_t i = 0; host_ok && i < host.size(); ++i) {
		const char ch = host[i];
		host_ok &= (ch >= 'a' && ch <= 'z')
			|| (ch >= 'A' && ch <= 'Z')
			|| (ch >= '0' && ch <= '9')
			|| (ch == '-')
			|| (ch == '.');
	}

	if (host_ok) {
		return host;
	}

	std::string new_host;
	std::vector<char> buffer;

	size_t start = 0;
	do {
		size_t next = host.find('.', start);
		if (next == std::string::npos)
			next = host.size();

		// Don't know what is a maximum length for punny-encoded string,
		// but 4-times bigger than original one should fit the requirements
		buffer.resize(host.size() * 4);

		size_t data_size = 0;
		size_t buffer_size = buffer.size();

		uint32_t *data = stringprep_utf8_to_ucs4(host.data() + start, next - start, &data_size);
		auto status = punycode_encode(data_size, data, NULL, &buffer_size, buffer.data());
		free(data);

		if (status == PUNYCODE_SUCCESS) {
			buffer.resize(buffer_size);

			new_host += "xn--";
			new_host.append(buffer.begin(), buffer.end());
			new_host += '.';
		} else {
			return host;
		}

		start = next + 1;
	} while (start < host.size());

	new_host.resize(new_host.size() - 1);
	return new_host;
}

//! Returns puny-decoded host if it's puny-encoded
static std::string decode_host(const std::string &host)
{
	if (host.empty()) {
		return host;
	}

	std::string new_host;
	std::vector<uint32_t> buffer;

	size_t start = 0;
	do {
		size_t next = host.find('.', start);
		if (next == std::string::npos)
			next = host.size();

		if (host.compare(start, 4, "xn--", 4) != 0) {
			new_host.append(host.begin() + start, host.begin() + next);
			new_host += '.';
			start = next + 1;
			continue;
		}

		// Don't know what is a maximum length for punny-decoded string,
		// but 4-times bigger than original one should fit the requirements
		// Especially if result is stored as UCS-4
		buffer.resize(host.size() * 4);

		size_t data_size = 0;
		size_t buffer_size = buffer.size();

		auto status = punycode_decode(next - start - 4, host.data() + start + 4, &buffer_size, buffer.data(), NULL);

		if (status == PUNYCODE_SUCCESS) {
			char *data = stringprep_ucs4_to_utf8(buffer.data(), buffer_size, NULL, &data_size);
			try {
				new_host.append(data, data + data_size);
				new_host += '.';
			} catch (...) {
				free(data);
				throw;
			}
			free(data);
		} else {
			return host;
		}

		start = next + 1;
	} while (start < host.size());

	if (new_host.size() > 1)
		new_host.resize(new_host.size() - 1);
	return new_host;
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

	size_t hostStart = tmp.find("//");
	size_t hostEnd = std::string::npos;
	size_t firstToFix = 0;

	if (hostStart != std::string::npos) {
		// Has host part, find delimiter
		hostStart += 2; // skip "//"
		hostEnd = tmp.find('/', hostStart);
		if (hostEnd == std::string::npos)
			hostEnd = tmp.find('#', hostStart);
		if (hostEnd == std::string::npos)
			hostEnd = tmp.find('?');
		if (hostEnd == std::string::npos)
			hostEnd = tmp.size() - 1;

		if (hostEnd == std::string::npos)
			hostEnd = tmp.size();

		std::string new_host = encode_host(tmp.substr(hostStart, hostEnd - hostStart));
		tmp.replace(tmp.begin() + hostStart, tmp.begin() + hostEnd, new_host);

		firstToFix = hostEnd;
	}

	// Reserved and unreserved characters are fine
	//         unreserved    = ALPHA / DIGIT / "-" / "." / "_" / "~"
	//         reserved      = gen-delims / sub-delims
	//         gen-delims    = ":" / "/" / "?" / "#" / "[" / "]" / "@"
	//         sub-delims    = "!" / "$" / "&" / "'" / "(" / ")"
	//                         / "*" / "+" / "," / ";" / "="
	// Replace everything else with percent encoding
	static const char doEncode[] = " \"<>[\\]^`{|}";
	for (size_t i = firstToFix; i < tmp.size(); ++i) {
		unsigned char ch = static_cast<unsigned char>(tmp[i]);
		if (ch < 32 || ch > 127 || strchr(doEncode, ch)) {
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

static std::string to_string_unescaped(const UriTextRangeA &range)
{
	if (!range.first || !range.afterLast)
		return std::string();

	std::vector<char> result(range.first, range.afterLast);
	result.push_back('\0');
	auto end = uriUnescapeInPlaceA(result.data());
	result.resize(end - result.data());

	return std::string(result.begin(), result.end());
}

static UriTextRangeA to_range(const std::string &str)
{
	UriTextRangeA range = {
		str.empty() ? NULL : str.c_str(),
		str.empty() ? NULL : (str.c_str() + str.size())
	};
	return range;
}

static UriTextRangeA to_range_escaped(const std::string &str, std::vector<std::vector<char>> &buffers)
{
	if (str.empty()) {
		return {
			NULL,
			NULL
		};
	}

	buffers.emplace_back();
	auto &buffer = buffers.back();

	buffer.resize(str.size() * 3 + 1);
	auto end = uriEscapeExA(str.data(), str.data() + str.size(), buffer.data(), true, false);

	return {
		buffer.data(),
		end
	};
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

	raw_query = to_string(uri.query);
	host = decode_host(to_string(uri.hostText));
	scheme = to_string(uri.scheme);
	fragment = to_string_unescaped(uri.fragment);

	path = std::string();

	if (uri.absolutePath || !scheme.empty()) {
		path += "/";
	}

	for (auto it = uri.pathHead; it; it = it->next) {
		if (it != uri.pathHead)
			path += "/";

		auto str = to_string_unescaped(it->text);

		path += str;
		path_components.push_back(str);
	}

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

		m_host = encode_host(url.host);

		m_uri.scheme = to_range(url.scheme);
		m_uri.hostText = to_range(m_host);
		m_uri.portText = to_range(m_port);
		m_uri.query = to_range(m_query);
		m_uri.fragment = to_range_escaped(url.fragment, m_buffers);

		if (url.path.compare(0, 1, "/", 1) == 0) {
			m_uri.absolutePath = true;
		} else {
			m_uri.absolutePath = false;
		}

		for (auto it = url.path_components.begin(); it != url.path_components.end(); ++it) {
			UriPathSegmentA segment = {
				to_range_escaped(*it, m_buffers),
				NULL,
				NULL
			};

			m_path_segments.push_back(segment);
		}

		for (size_t i = 1; i < m_path_segments.size(); ++i) {
			m_path_segments[i - 1].next = &m_path_segments[i];
		}

		m_uri.pathHead = &m_path_segments[0];
	}

	uri_generator(const uri_generator &other) = delete;
	uri_generator &operator =(const uri_generator &other) = delete;

	const UriUriA *uri() const
	{
		return &m_uri;
	}

private:
	UriUriA m_uri;
	std::string m_host;
	std::string m_query;
	std::string m_port;
	std::vector<UriPathSegmentA> m_path_segments;
	std::vector<std::vector<char>> m_buffers;
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

std::string url::to_human_readable() const
{
	if (!is_valid()) {
		return std::string();
	}

	if (p->state & url_private::has_human_readable) {
		return p->human_readable;
	}

	std::string result;

	if (!p->scheme.empty()) {
		result += p->scheme;
		result += ":";
	}

	if (!p->host.empty()) {
		result += "//";
		result += p->host;

		if (!p->path.empty() && p->path[0] != '/')
			result += "/";
	}

	if (!p->path.empty()) {
		result += p->path;
	}

	const url_query &query = url::query();
	if (query.count() > 0) {
		result += "?";
		for (size_t i = 0; i < query.count(); ++i) {
			if (i > 0)
				result += "&";
			auto pair = query.item(i);
			result += pair.first;
			result += "=";
			result += pair.second;
		}
	}

	if (!p->fragment.empty()) {
		result += "#";
		result += p->fragment;
	}

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

	//std::cout << "valid: " << is_valid() << ", path: " << path() << ", compare: " << (path().compare(0, 1, "/", 1) != 0) << std::endl;

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
	p->path_components.clear();

	boost::split(p->path_components, path, boost::is_any_of("/"));
	if (!path.empty() && path.at(0) == '/')
		p->path_components.erase(p->path_components.begin());
}

const std::vector<std::string> &url::path_components() const
{
	p->ensure_data();
	return p->path_components;
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
		p->state &= ~url_private::has_human_readable;
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
