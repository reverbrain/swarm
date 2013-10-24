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

#ifndef IOREMAP_THEVOID_STOCKREPLIES_P_HPP
#define IOREMAP_THEVOID_STOCKREPLIES_P_HPP

#include <boost/asio/buffer.hpp>
#include <swarm/http_response.hpp>

namespace ioremap {
namespace thevoid {

namespace stock_replies
{

boost::asio::const_buffer status_to_buffer(swarm::http_response::status_type status);
swarm::http_response stock_reply(swarm::http_response::status_type status);
std::vector<boost::asio::const_buffer> to_buffers(const swarm::http_response &reply, const boost::asio::const_buffer &content);
void to_buffers(const swarm::http_response &reply, std::vector<char> &buffer);

}

} } // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_STOCKREPLIES_P_HPP
