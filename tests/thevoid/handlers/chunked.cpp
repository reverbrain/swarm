/*
 * Copyright 2016+ Evgeniy Polyakov <zbr@ioremap.net>
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

#include <memory>

#include "thevoid/stream.hpp"

#include "handlers_factory.hpp"

using namespace ioremap;
namespace handlers {

class chunked: public thevoid::buffered_request_stream<server>, public std::enable_shared_from_this<chunked>
{
	virtual void on_request(const thevoid::http_request &req) {
		(void) req;

		this->try_next_chunk();
	}

	virtual void on_chunk(const boost::asio::const_buffer &buffer, unsigned int flags) {
		auto data = boost::asio::buffer_cast<const char*>(buffer);
		size_t size = boost::asio::buffer_size(buffer);

		m_data.insert(m_data.end(), data, data + size);

		if (flags & this->last_chunk) {
			thevoid::http_response reply;
			reply.set_code(thevoid::http_response::ok);
			reply.headers().set_content_length(m_data.size());
			reply.headers().set("X-Total-Size", std::to_string(m_data.size()));

			this->send_reply(std::move(reply), std::move(m_data));
		} else {
			this->try_next_chunk();
		}
	}

	virtual void on_error(const boost::system::error_code &err) {
		(void) err;
	}

	std::vector<char> m_data;
};

} // namespace handlers

REGISTER_HANDLER(chunked)
