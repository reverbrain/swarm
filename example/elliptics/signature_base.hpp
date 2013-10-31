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

#ifndef IOREMAP_THEVOID_ELLIPTICS_SIGNATURE_BASE_HPP
#define IOREMAP_THEVOID_ELLIPTICS_SIGNATURE_BASE_HPP

// must be the first, since thevoid internally uses X->boost::buffer conversion,
// which must be present at compile time
#include "asio.hpp"

#include <thevoid/server.hpp>
#include <swarm/logger.hpp>

namespace ioremap {
namespace thevoid {
namespace elliptics {

class signature_base
{
public:
    signature_base();

    bool initialize(const rapidjson::Value &config, const ioremap::elliptics::node &node, const swarm::logger &logger);

    std::string sign(const swarm::url &url) const;

protected:
    swarm::logger logger() const;

private:
    std::string m_key;
    std::unique_ptr<ioremap::elliptics::node> m_node;
    swarm::logger m_logger;
};

} // namespace elliptics
} // namespace thevoid
} // namespace ioremap

#endif // IOREMAP_THEVOID_ELLIPTICS_SIGNATURE_BASE_HPP
