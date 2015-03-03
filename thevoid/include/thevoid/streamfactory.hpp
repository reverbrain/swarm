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

#ifndef IOREMAP_THEVOID_STREAMFACTORY_HPP
#define IOREMAP_THEVOID_STREAMFACTORY_HPP

#include <thevoid/stream.hpp>

namespace ioremap {
namespace thevoid {

class base_stream_factory
{
public:
	base_stream_factory();
	virtual ~base_stream_factory();

	virtual std::shared_ptr<base_request_stream> create() = 0;
};

template <typename Server, typename T>
class stream_factory : public base_stream_factory
{
public:
	stream_factory(Server *server) : m_server(server) {}
	~stream_factory() /*override*/ {}

	std::shared_ptr<base_request_stream> create() /*override*/
	{
		if (__builtin_expect(!m_server, false))
			throw std::logic_error("stream_factory::m_server is null pointer");

		auto stream = std::make_shared<T>();
		stream->set_server(m_server);
		return stream;
	}

private:
	Server *m_server;
};

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_STREAMFACTORY_HPP
