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

class ok
	: public ioremap::thevoid::simple_request_stream<server>
	, public std::enable_shared_from_this<ok>
{
	virtual void on_request(const ioremap::thevoid::http_request& /* req */,
			const boost::asio::const_buffer& /* buffer */)
	{
		this->send_reply(ioremap::thevoid::http_response::ok);
	}
};

} // namespace handlers

REGISTER_HANDLER(ok)
