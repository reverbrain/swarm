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

class network_url_private
{
public:
	std::string url;
	UriParserStateA state;
	UriUriA uri;
	network_url_cleaner cleaner;

	bool is_hex(char ch)
	{
		return ((ch >= '0' && ch <= '9')
			|| (ch >= 'a' && ch <= 'f')
			|| (ch >= 'A' && ch <= 'F'));
	}

	char to_hex(char value)
	{
		static const char hex[] = "0123456789abcdef";
		return hex[value];
	}

	std::string encode_url(const std::string &url)
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
		for (int i = 0; i < tmp.size(); ++i) {
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
};

network_url::network_url() : p(new network_url_private)
{
}

network_url::~network_url()
{
}

bool network_url::set_base(const std::string &url)
{
	p->cleaner.reset(NULL);
	p->url = p->encode_url(url);
	p->state.uri = &p->uri;
	if (uriParseUriA(&p->state, p->url.c_str()) != URI_SUCCESS) {
		return false;
	}
	p->cleaner.reset(&p->uri);
	return true;
}

static std::string to_string(const UriTextRangeA &range)
{
	if (!range.first || !range.afterLast)
		return std::string();
	return std::string(range.first, range.afterLast);
}

static std::string network_url_normalized(UriUriA *uri)
{
	const unsigned int dirtyParts = uriNormalizeSyntaxMaskRequiredA(uri);
	if (uriNormalizeSyntaxExA(uri, dirtyParts) != URI_SUCCESS) {
		return std::string();
	}

	int charsRequired = 0;

	UriTextRangeA &url_range = uri->fragment;
	size_t fragment_size = url_range.afterLast - url_range.first;

	if (uriToStringCharsRequiredA(uri, &charsRequired) != URI_SUCCESS)
		return std::string();

	std::string result;
	result.resize(charsRequired + 1);

	if (uriToStringA(&result[0], uri, charsRequired + 1, NULL) != URI_SUCCESS)
		return std::string();

	result.resize(charsRequired - fragment_size);
	if (result[result.size() - 1] == '#')
		result.resize(result.size() - 1);

	return result;
}

std::string network_url::normalized()
{
	return network_url_normalized(&p->uri);
}

std::string network_url::host()
{
	return to_string(p->uri.hostText);
}

std::string network_url::path()
{
	std::string path;

	if (p->uri.absolutePath) {
		path += "/";
	}

	for (auto it = p->uri.pathTail; it; it = it->next) {
		path += to_string(it->text);
	}

	return path;
}

std::string network_url::relative(const std::string &other, std::string *other_host)
{
	std::string url = p->encode_url(other);

	UriUriA absolute_destination;
	UriUriA relative_source;

	network_url_cleaner destination_cleaner;
	network_url_cleaner source_cleaner;

	p->state.uri = &relative_source;
	if (uriParseUriA(&p->state, url.c_str()) != URI_SUCCESS)
		return std::string();
	source_cleaner.reset(&relative_source);

	if (uriAddBaseUriA(&absolute_destination, &relative_source, &p->uri) != URI_SUCCESS)
		return std::string();
	destination_cleaner.reset(&absolute_destination);

	if (other_host)
		*other_host = to_string(absolute_destination.hostText);
	return network_url_normalized(&absolute_destination);
}

} // namespace crawler
} // namespace cocaine
