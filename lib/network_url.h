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

#ifndef COCAINE_CRAWLER_NETWORK_URL_H
#define COCAINE_CRAWLER_NETWORK_URL_H

#include <string>
#include <memory>

namespace ioremap {
namespace swarm {

class network_url_private;

class network_url
{
public:
    network_url();
    ~network_url();

    bool set_base(const std::string &url);

    std::string normalized();
    std::string host();
    std::string relative(const std::string &other, std::string *other_host = NULL);

private:
    network_url(const network_url &other);
    network_url &operator =(const network_url &other);

    std::unique_ptr<network_url_private> p;
};

} // namespace crawler
} // namespace cocaine

#endif // COCAINE_CRAWLER_NETWORK_URL_H
