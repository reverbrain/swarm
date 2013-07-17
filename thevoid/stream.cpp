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

#include "stream.hpp"

namespace ioremap {
namespace thevoid {

reply_stream::reply_stream()
{
}

reply_stream::~reply_stream()
{
}

base_request_stream::base_request_stream()
{
}

base_request_stream::~base_request_stream()
{
}

void base_request_stream::initialize(const std::shared_ptr<reply_stream> &reply)
{
	m_reply = reply;
}

} // namespace thevoid
} // namespace ioremap
