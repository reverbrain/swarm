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

#include "stream_p.hpp"

namespace ioremap {
namespace thevoid {

reply_stream::reply_stream()
{
}

reply_stream::~reply_stream()
{
}

void reply_stream::virtual_hook(reply_stream::reply_stream_hook id, void *data)
{
	(void) id;
	(void) data;
}

blackhole::log::attributes_t *reply_stream::get_logger_attributes()
{
	get_logger_attributes_hook_data result;
	result.data = NULL;
	virtual_hook(get_logger_attributes_hook, &result);
	return result.data;
}

base_request_stream::base_request_stream() : m_data(new base_request_stream_data)
{
}

base_request_stream::~base_request_stream()
{
}

void base_request_stream::initialize(const std::shared_ptr<reply_stream> &reply)
{
	m_reply = reply;
	m_logger.reset(new swarm::logger(reply->create_logger()));
	m_data->logger_attributes = reply->get_logger_attributes();
}

void base_request_stream::virtual_hook(base_request_stream::request_stream_hook id, void *data)
{
	(void) id;
	(void) data;
}

blackhole::log::attributes_t *base_request_stream::logger_attributes()
{
	return m_data->logger_attributes;
}

} // namespace thevoid
} // namespace ioremap
