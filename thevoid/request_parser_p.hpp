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

#ifndef IOREMAP_THEVOID_REQUEST_PARSER_HPP
#define IOREMAP_THEVOID_REQUEST_PARSER_HPP

#include <boost/logic/tribool.hpp>
#include <boost/tuple/tuple.hpp>
#include <swarm/networkrequest.h>

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
