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
