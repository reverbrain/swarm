//
// request_parser.hpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

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

	boost::tuple<boost::tribool, const char *> parse_new(
		swarm::network_request &req, const char *begin, const char *end);

    boost::tribool parse_line(swarm::network_request &request, const std::string &line);

	//! Parse some data. The tribool return value is true when a complete request
	//! has been parsed, false if the data is invalid, indeterminate when more
	//! data is required. The InputIterator return value indicates how much of the
	//! input has been consumed.
	template <typename InputIterator>
	boost::tuple<boost::tribool, InputIterator> parse(swarm::network_request &req,
		InputIterator begin, InputIterator end)
	{
		while (begin != end) {
			boost::tribool result = consume(req, *begin++);
			if (result || !result)
				return boost::make_tuple(result, begin);
		}
		boost::tribool result = boost::indeterminate;
		return boost::make_tuple(result, begin);
	}

private:
	//! Handle the next character of input.
	inline boost::tribool consume(swarm::network_request &req, char input) __attribute__((always_inline))
	{
		switch (m_state) {
			case method_start:
				if (input == '\r' || input == '\n') {
                    // Ignore CRLF characters between requests in Keep-Alive connection
					return boost::indeterminate;
				} else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
					return false;
                } else {
					m_state = method;
					m_method.push_back(input);
					return boost::indeterminate;
				}
			case method:
				if (input == ' ') {
                    req.set_method(m_method);
					m_state = uri;
					return boost::indeterminate;
				} else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
					return false;
				} else {
					m_method.push_back(input);
					return boost::indeterminate;
				}
			case uri:
				if (input == ' ') {
                    req.set_url(m_uri);
					m_state = http_version_h;
					return boost::indeterminate;
				} else if (is_ctl(input)) {
					return false;
				} else {
					m_uri.push_back(input);
					return boost::indeterminate;
				}
			case http_version_h:
				if (input == 'H') {
					m_state = http_version_t_1;
					return boost::indeterminate;
				} else {
					return false;
				}
			case http_version_t_1:
				if (input == 'T') {
					m_state = http_version_t_2;
					return boost::indeterminate;
				} else {
					return false;
				}
			case http_version_t_2:
				if (input == 'T') {
					m_state = http_version_p;
					return boost::indeterminate;
				} else {
					return false;
				}
			case http_version_p:
				if (input == 'P') {
					m_state = http_version_slash;
					return boost::indeterminate;
				} else {
					return false;
				}
			case http_version_slash:
				if (input == '/') {
                    m_http_version_major = 0;
                    m_http_version_minor = 0;
					m_state = http_version_major_start;
					return boost::indeterminate;
				} else {
					return false;
				}
			case http_version_major_start:
				if (is_digit(input)) {
					m_http_version_major = m_http_version_major * 10 + input - '0';
					m_state = http_version_major;
					return boost::indeterminate;
				} else {
					return false;
				}
			case http_version_major:
				if (input == '.') {
					m_state = http_version_minor_start;
					return boost::indeterminate;
				} else if (is_digit(input)) {
					m_http_version_major = m_http_version_major * 10 + input - '0';
					return boost::indeterminate;
				} else {
					return false;
				}
			case http_version_minor_start:
				if (is_digit(input)) {
					m_http_version_minor = m_http_version_minor * 10 + input - '0';
					m_state = http_version_minor;
					return boost::indeterminate;
				} else {
					return false;
				}
			case http_version_minor:
				if (input == '\r') {
                    req.set_http_version(m_http_version_major, m_http_version_minor);
					m_state = expecting_newline_1;
					return boost::indeterminate;
				} else if (is_digit(input)) {
					m_http_version_minor = m_http_version_minor * 10 + input - '0';
					return boost::indeterminate;
				} else {
					return false;
				}
			case expecting_newline_1:
				if (input == '\n') {
					m_state = header_line_start;
					return boost::indeterminate;
				} else {
					return false;
				}
			case header_line_start:
				if (input == '\r') {
					m_state = expecting_newline_3;
					return boost::indeterminate;
				} else if (!req.get_headers().empty() && (input == ' ' || input == '\t')) {
					m_state = header_lws;
					return boost::indeterminate;
				} else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
					return false;
				} else {
                    m_header.first.resize(0);
                    m_header.second.resize(0);

                    m_header.first.push_back(input);

					m_state = header_name;
					return boost::indeterminate;
				}
			case header_lws:
				if (input == '\r') {
					m_state = expecting_newline_2;
					return boost::indeterminate;
				} else if (input == ' ' || input == '\t') {
					return boost::indeterminate;
				} else if (is_ctl(input)) {
					return false;
				} else {
					m_state = header_value;

                    m_header.second.push_back(input);
					return boost::indeterminate;
				}
			case header_name:
				if (input == ':') {
					m_state = space_before_header_value;
					return boost::indeterminate;
				} else if (!is_char(input) || is_ctl(input) || is_tspecial(input)) {
					return false;
				} else {
                    m_header.first.push_back(input);
					return boost::indeterminate;
				}
			case space_before_header_value:
				if (input == ' ') {
					m_state = header_value;
					return boost::indeterminate;
				} else {
					return false;
				}
			case header_value:
				if (input == '\r') {
                    req.add_header(m_header);
					m_state = expecting_newline_2;
					return boost::indeterminate;
				} else if (is_ctl(input)) {
					return false;
				} else {
                    m_header.second.push_back(input);
					return boost::indeterminate;
				}
			case expecting_newline_2:
				if (input == '\n') {
					m_state = header_line_start;
					return boost::indeterminate;
				} else {
					return false;
				}
			case expecting_newline_3:
				return (input == '\n');
			default:
				return false;
		}
	}

	//! Check if a byte is an HTTP character.
	inline static bool is_char(int c)
	{
		return c >= 0 && c <= 127;
	}

	//! Check if a byte is an HTTP control character.
	inline static bool is_ctl(int c)
	{
		return (c >= 0 && c <= 31) || (c == 127);
	}

	//! Check if a byte is defined as an HTTP tspecial character.
	inline static bool is_tspecial(int c)
	{
		switch (c) {
			case '(': case ')': case '<': case '>': case '@':
			case ',': case ';': case ':': case '\\': case '"':
			case '/': case '[': case ']': case '?': case '=':
			case '{': case '}': case ' ': case '\t':
				return true;
			default:
				return false;
		}
	}

	//! Check if a byte is a digit.
	inline static bool is_digit(int c)
	{
		return c >= '0' && c <= '9';
	}

	//! The current state of the parser.
	enum state
	{
		method_start,
		method,
		uri,
		http_version_h,
		http_version_t_1,
		http_version_t_2,
		http_version_p,
		http_version_slash,
		http_version_major_start,
		http_version_major,
		http_version_minor_start,
		http_version_minor,
		expecting_newline_1,
		header_line_start,
		header_lws,
		header_name,
		space_before_header_value,
		header_value,
		expecting_newline_2,
		expecting_newline_3
	} m_state;

	enum state_new
	{
		request_line,
		header_line
	} m_state_new;

    std::string m_method;
    std::string m_uri;
    std::string m_line;
    swarm::headers_entry m_header;
    int m_http_version_major;
	int m_http_version_minor;
};

} // namespace ioremap
} // namespace thevoid

#endif // IOREMAP_THEVOID_REQUEST_PARSER_HPP
