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

class echo
	: public ioremap::thevoid::request_stream<server>
	, public std::enable_shared_from_this<echo>
{
	virtual void on_headers(ioremap::thevoid::http_request&& req) {
		if (auto content_length = req.headers().content_length()) {
			auto& url_query = req.url().query();
			ioremap::thevoid::http_response response;

			auto response_code = url_query.item_value<int>(
					"code",
					ioremap::thevoid::http_response::HTTP_200_OK
				);
			response.set_code(response_code);

			auto response_reason = url_query.item_value<std::string>(
					"reason",
					ioremap::thevoid::http_response::default_reason(response_code)
				);
			response.set_reason(response_reason);

			response.headers().set_content_length(*content_length);

			this->send_headers(std::move(response), ioremap::thevoid::reply_stream::result_function());
		}
		else {
			this->reply()->send_error(ioremap::thevoid::http_response::HTTP_400_BAD_REQUEST);
		}
	}

	virtual size_t on_data(const boost::asio::const_buffer& buffer) {
		auto data_size = boost::asio::buffer_size(buffer);
		this->send_data(buffer, ioremap::thevoid::reply_stream::result_function());
		return data_size;
	}

	virtual void on_close(const boost::system::error_code& err) {
		// if there was error, reply stream will be closed anyway
		if (!err) {
			reply()->close(err);
		}
	}
};

} // namespace handlers

REGISTER_HANDLER(echo)
