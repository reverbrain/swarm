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

#ifndef IOREMAP_THEVOID_REQUEST_PARSER_HPP
#define IOREMAP_THEVOID_REQUEST_PARSER_HPP

#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>
#include <swarm/http_request.hpp>

namespace ioremap {
namespace thevoid {

//! Parser for incoming requests.
class request_parser
{
public:
	//! Construct ready to parse the request method.
	request_parser();

	//! Reset to initial parser state.
	void reset();
    
    //! Parse some data. The tribool return value is true when a complete request
	//! has been parsed, false if the data is invalid, indeterminate when more
	//! data is required. The InputIterator return value indicates how much of the
	//! input has been consumed.
	boost::tuple<boost::tribool, const char *> parse(
		swarm::http_request &req, const char *begin, const char *end);

    boost::tribool parse_line(swarm::http_request &request, const std::string &line);


	enum state_new
	{
		request_line,
		header_line
	} m_state_new;

    std::string m_line;
    swarm::headers_entry m_header;
};

} // namespace ioremap
} // namespace thevoid

#endif // IOREMAP_THEVOID_REQUEST_PARSER_HPP
