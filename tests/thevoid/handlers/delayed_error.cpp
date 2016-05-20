/*
 * Copyright 2015+ Danil Osherov <shindo@yandex-team.ru>
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


namespace handlers {

class delayed_error
	: public ioremap::thevoid::buffered_request_stream<server>
	, public std::enable_shared_from_this<delayed_error>
{
	virtual void on_request(const ioremap::thevoid::http_request& req)
	{
		code = req.url().query().item_value<int>("code", 403);
		delay_size = req.url().query().item_value<size_t>("delay", -1);
		response_data = req.url().query().item_value<std::string>("response", "");

		auto chunk = req.url().query().item_value<int>("chunk", 1024);
		set_chunk_size(chunk);

		received = 0;

		auto content_length = req.headers().content_length().get_value_or(0);

		if (content_length == 0 || delay_size == 0) {
			// send response right here
			send_response();
		} else {
			try_next_chunk();
		}
	}

	virtual void on_chunk(const boost::asio::const_buffer& buffer, unsigned int flags) {
		received += boost::asio::buffer_size(buffer);

		if (received >= delay_size || (flags & last_chunk)) {
			send_response();
		} else {
			try_next_chunk();
		}
	}

	virtual void on_error(const boost::system::error_code& /* err */) {
	}

private:
	void send_response() {
		ioremap::thevoid::http_response response;
		response.set_code(code);
		response.headers().set_content_length(response_data.size());
		response.headers().set_keep_alive(false);

		send_reply(std::move(response), std::move(response_data));
	}

	int code;
	size_t delay_size;
	std::string response_data;
	size_t received;
};

} // namespace handlers

REGISTER_HANDLER(delayed_error)
