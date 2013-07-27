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

#include "networkrequest.h"

#define EV_MULTIPLICITY 1
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wstrict-aliasing"

#include <ev++.h>
#pragma GCC diagnostic pop

#include <memory>
#include <functional>
#include <map>

namespace ioremap {
namespace swarm {

class network_manager_private;

class network_manager
{
public:
    network_manager(ev::loop_ref &loop);
    ~network_manager();

    void set_limit(int active_connections);

    void get(const std::function<void (const network_reply &reply)> &handler, const network_request &request);
    void post(const std::function<void (const network_reply &reply)> &handler, const network_request &request, const std::string &body);

private:
    network_manager(const network_manager &other);
    network_manager &operator =(const network_manager &other);

    network_manager_private *p;

    friend class network_manager_private;
};

} // namespace service
} // namespace cocaine

#endif // COCAINE_SERVICE_NETWORKMANAGER_H
