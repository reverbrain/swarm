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

#include "stream.hpp"

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
    stream_factory(const std::shared_ptr<Server> &server) : m_server(server) {}
    ~stream_factory() /*override*/ {}

    std::shared_ptr<base_request_stream> create() /*override*/
    {
        if (auto server = m_server.lock()) {
            auto stream = std::make_shared<T>();
            stream->set_server(server);
            return stream;
        }

        throw std::logic_error("stream_factory::m_server is null pointer");
    }

private:
    std::weak_ptr<Server> m_server;
};

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_STREAMFACTORY_HPP
