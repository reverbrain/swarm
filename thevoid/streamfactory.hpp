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
