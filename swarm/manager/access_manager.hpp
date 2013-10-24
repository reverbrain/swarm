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

#ifndef COCAINE_SERVICE_NETWORKMANAGER_H
#define COCAINE_SERVICE_NETWORKMANAGER_H

#include "../http_response.hpp"
#include "../http_request.hpp"

#include "../logger.hpp"
#include "event_loop.hpp"
#include <memory>
#include <functional>
#include <map>

namespace ioremap {
namespace swarm {

class network_manager_private;

class access_manager
{
public:
    access_manager(event_loop &loop, const ioremap::swarm::logger &logger);
    ~access_manager();

    void set_limit(int active_connections);
    void set_logger(const swarm::logger &log);
    swarm::logger logger() const;

    void get(const std::function<void (const http_response &reply)> &handler, const http_request &request);
    void post(const std::function<void (const http_response &reply)> &handler, const http_request &request, const std::string &body);

private:
    access_manager(const access_manager &other);
    access_manager &operator =(const access_manager &other);

    network_manager_private *p;

    friend class network_manager_private;
};

} // namespace service
} // namespace cocaine

#endif // COCAINE_SERVICE_NETWORKMANAGER_H
