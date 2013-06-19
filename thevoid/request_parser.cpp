//
// request_parser.cpp
// ~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2012 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "request_parser_p.hpp"

namespace ioremap {
namespace thevoid {

request_parser::request_parser()
	: m_state(method_start)
{
    m_uri.reserve(32);
    m_method.reserve(32);
    m_header.first.reserve(32);
    m_header.second.reserve(32);
}

void request_parser::reset()
{
	m_state = method_start;
    m_header.first.resize(0);
    m_header.second.resize(0);
    m_method.resize(0);
    m_uri.resize(0);
}

} // namespace ioremap
} // namespace thevoid
