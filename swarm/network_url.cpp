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

#include "network_url.h"
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
		has_changes     = 0x08
	};

	url_private() : state(invalid), port(-1)
	{
	}

	url_private(const url_private &other) = default;

	void ensure_data() const;

	mutable std::string scheme;
	mutable std::string user_name;
	mutable std::string password;
	mutable std::string host;
	mutable std::string path;
        mutable std::string query;
        mutable std::string fragment;

	std::string original;

	mutable int state;
	mutable int port;
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

	UriParserStateA parser_state;
	UriUriA parser;
	network_url_cleaner cleaner;

	if (uriParseUriA(&parser_state, original.c_str()) != URI_SUCCESS) {
		state |= (parsed | invalid_original);
		return;
	}

	cleaner.reset(&parser);

	state |= parsed;

	try {
		std::string port_text = to_string(parser.portText);
		port = boost::lexical_cast<int>(port_text);
	} catch (...) {
		port = -1;
		state |= invalid;
		return;
	}

	if (parser.absolutePath) {
		path += "/";
	}

	for (auto it = parser.pathHead; it; it = it->next) {
		if (it != parser.pathHead)
			path += "/";
		path += to_string(it->text);
	}

	query = to_string(parser.query);
	host = to_string(parser.hostText);
	scheme = to_string(parser.scheme);
	fragment = to_string(parser.fragment);
}

class network_url_private
{
public:
	std::string url;
	UriParserStateA state;
	UriUriA uri;
	network_url_cleaner cleaner;
};

url::url() : p(new url_private)
{
}

url::url(url &&other) : p(std::move(other.p))
{
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
	p = std::move(other.p);
	return *this;
}

url &url::operator =(const url &other)
{
	url tmp(other);
	*this = std::move(tmp);
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

std::string url::to_string() const
{
	if (!is_valid())
		return std::string();

	if (p->state & url_private::has_changes) {
		// FIXME: apply all changes by constructing new string
		// And don't forget about normalization! We are not stupid bots :)
	}
	return p->original;
}

bool url::is_valid() const
{
	p->ensure_data();
	return !((p->state & url_private::invalid_original) || (p->state == url_private::invalid));
}

const std::string &url::scheme() const
{
	p->ensure_data();
	return p->scheme;
}

//static std::string network_url_normalized(UriUriA *uri)
//{
//	const unsigned int dirtyParts = uriNormalizeSyntaxMaskRequiredA(uri);
//	if (uriNormalizeSyntaxExA(uri, dirtyParts) != URI_SUCCESS) {
//		return std::string();
//	}

//	int charsRequired = 0;

//	UriTextRangeA &url_range = uri->fragment;
//	size_t fragment_size = url_range.afterLast - url_range.first;

//	if (uriToStringCharsRequiredA(uri, &charsRequired) != URI_SUCCESS)
//		return std::string();

//	std::string result;
//	result.resize(charsRequired + 1);

//	if (uriToStringA(&result[0], uri, charsRequired + 1, NULL) != URI_SUCCESS)
//		return std::string();

//	result.resize(charsRequired - fragment_size);
//	if (result[result.size() - 1] == '#')
//		result.resize(result.size() - 1);

//	return result;
//}

//std::string url::normalized()
//{
//	return network_url_normalized(&parser);
//}

const std::string &url::host() const
{
	p->ensure_data();
	return p->host;
}

int url::port() const
{
	p->ensure_data();
	return p->port;
}

const std::string &url::path() const
{
	p->ensure_data();
	return p->path;
}

//std::string url::relative(const std::string &other, std::string *other_host) const
//{
//	std::string url = p->encode_url(other);

//	UriUriA absolute_destination;
//	UriUriA relative_source;

//	network_url_cleaner destination_cleaner;
//	network_url_cleaner source_cleaner;

//	p->state.uri = &relative_source;
//	if (uriParseUriA(&p->state, url.c_str()) != URI_SUCCESS)
//		return std::string();
//	source_cleaner.reset(&relative_source);

//	if (uriAddBaseUriA(&absolute_destination, &relative_source, &parser) != URI_SUCCESS)
//		return std::string();
//	destination_cleaner.reset(&absolute_destination);

//	if (other_host)
//		*other_host = to_string(absolute_destination.hostText);
//	return network_url_normalized(&absolute_destination);
//	return std::string();
//}

const std::string &url::query() const
{
	p->ensure_data();
	return p->query;
}

const std::string &url::fragment() const
{
	p->ensure_data();
	return p->fragment;
}

} // namespace crawler
} // namespace cocaine
