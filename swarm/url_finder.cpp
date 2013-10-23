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

#include "url_finder.hpp"
#include <libxml/HTMLparser.h>
#include <cstring>

namespace ioremap {
namespace swarm {


struct parser_context
{
	std::vector<std::string> &urls;
};

static void parser_start_element(void *void_context,
				 const xmlChar *tag_name,
				 const xmlChar **attributes)
{
	parser_context *context = reinterpret_cast<parser_context*>(void_context);

	if (strcasecmp(reinterpret_cast<const char*>(tag_name), "a") == 0) {
		if (attributes) {
			for (size_t index = 0; attributes[index]; index += 2) {
				const xmlChar *name = attributes[index];
				const xmlChar *value = attributes[index + 1];
				if (!value)
					continue;

				if (strcmp(reinterpret_cast<const char*>(name), "href") == 0) {
					context->urls.push_back(reinterpret_cast<const char*>(value));
				}
			}
		}
	}
}

url_finder::url_finder(const std::string &html) : m_html(html), m_parsed(false)
{
}

const std::vector<std::string> &url_finder::urls() const
{
	if (!m_parsed) {
		m_parsed = true;
		parse();
	}

	return m_urls;
}

void url_finder::parse() const
{
	htmlParserCtxtPtr ctxt;
	parser_context context = { m_urls };

	htmlSAXHandler handler;
	memset(&handler, 0, sizeof(handler));
	handler.startElement = parser_start_element;

	ctxt = htmlCreatePushParserCtxt(&handler, &context, "", 0, "", XML_CHAR_ENCODING_NONE);

	htmlParseChunk(ctxt, m_html.c_str(), m_html.size(), 0);
	htmlParseChunk(ctxt, "", 0, 1);

	htmlFreeParserCtxt(ctxt);
}

} // namespace crawler
} // namespace cocaine
