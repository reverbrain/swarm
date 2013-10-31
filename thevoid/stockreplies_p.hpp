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
