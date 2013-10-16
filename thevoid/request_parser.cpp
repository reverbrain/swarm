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

#include "request_parser_p.hpp"

namespace ioremap {
namespace thevoid {

request_parser::request_parser() : m_state_new(request_line)
{
	m_header.first.reserve(32);
	m_header.second.reserve(32);
	m_line.reserve(32);
}

void request_parser::reset()
{
	m_state_new = request_line;
	m_header.first.resize(0);
	m_header.second.resize(0);
	m_line.resize(0);
}

template <typename Iterator>
Iterator find_crlf(Iterator begin, Iterator end)
{
	while (begin != end) {
		auto current = begin;
		auto next = ++begin;
		if (*current == '\r' && *next == '\n')
			return next;
	}
	return end;
}

boost::tuple<boost::tribool, const char *> request_parser::parse(
	swarm::network_request &request, const char *begin, const char *end)
{
	while (begin != end) {
		auto line_end = find_crlf(begin, end);
		m_line.append(begin, line_end);

		if (line_end == end)
			return boost::make_tuple(boost::tribool(boost::indeterminate), end);

		begin = line_end + 1;

		auto result = parse_line(request, m_line);
		if (result || !result)
			return boost::make_tuple(result, begin);

		m_line.resize(0);
	}

	return boost::make_tuple(boost::tribool(boost::indeterminate), end);
}

static inline int parse_int(const char *begin, const char *end, boost::tribool &result)
{
	int value = 0;
	while (begin != end) {
		if (!isdigit(*begin)) {
			result = false;
			break;
		}

		value = value * 10 + (*begin++) - '0';
	}

	return value;
}

template <typename Iter>
static inline void trim_line(Iter &begin, Iter &end)
{
	while (begin < end && isspace(*begin))
            ++begin;
        while (begin < end && isspace(*(end - 1)))
            --end;
}

boost::tribool request_parser::parse_line(swarm::network_request &request, const std::string &line)
{
	if (line.empty() || line[line.size() - 1] != '\r')
		return false;

	const bool is_empty_line = line.compare(0, line.size(), "\r", 1) == 0;

	switch (m_state_new) {
		case request_line: {
			if (is_empty_line) {
				// Ignore CRLF characters between requests in Keep-Alive connection
				return boost::indeterminate;
			}

			const auto first_space = line.find(' ');
			if (first_space == std::string::npos)
				return false;

			request.set_method(line.substr(0, first_space));

			const auto second_space = line.find(' ', first_space + 1);
			if (second_space == std::string::npos)
				return false;

			request.set_url(line.substr(first_space + 1, second_space - first_space - 1));

			if (line.compare(second_space + 1, 5, "HTTP/", 5) != 0)
				return false;

			const auto version_major_start = second_space + 1 + 5;
			const auto dot = line.find('.', version_major_start);
			if (dot == std::string::npos)
				return false;

			const auto version_minor_end = line.find('\r', dot);

			if (version_minor_end == std::string::npos)
				return false;

			boost::tribool result = boost::indeterminate;
			const auto major_version = parse_int(line.data() + version_major_start, line.data() + dot, result);
			const auto minor_version = parse_int(line.data() + dot + 1, line.data() + version_minor_end, result);
			request.set_http_version(major_version, minor_version);

			m_state_new = header_line;
			return result;
		}
		case header_line: {
			if (!m_header.first.empty() && (line[0] == ' ' || line[0] == '\t')) {
				// any number of LWS is allowed after field, rfc 2068
				auto begin = line.begin() + 2;
				auto end = line.end();
				trim_line(begin, end);

				m_header.second += ' ';
				m_header.second.append(begin, end);

				return boost::indeterminate;
			}

			if (!m_header.first.empty()) {
				request.add_header(m_header);
				m_header.first.resize(0);
				m_header.second.resize(0);
			}

			if (is_empty_line) {
				return true;
			}

			const auto colon = line.find(':');
			if (colon == std::string::npos)
				return false;

			auto name_begin = line.begin();
			auto name_end = line.begin() + colon;
			trim_line(name_begin, name_end);
			m_header.first.assign(name_begin, name_end);

			auto value_begin = line.begin() + colon + 1;
			auto value_end = line.end();
			trim_line(value_begin, value_end);
			m_header.second.assign(value_begin, value_end);

			return boost::indeterminate;
		}
	}

	return false;
}

} // namespace ioremap
} // namespace thevoid
