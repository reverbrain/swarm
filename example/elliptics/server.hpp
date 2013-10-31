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

#ifndef __IOREMAP_THEVOID_ELLIPTICS_SERVER_HPP
#define __IOREMAP_THEVOID_ELLIPTICS_SERVER_HPP

// must be the first, since thevoid internally uses X->boost::buffer conversion,
// which must be present at compile time
#include "asio.hpp"

#include <thevoid/server.hpp>
#include <swarm/logger.hpp>

#include "common.hpp"
#include "io.hpp"
#include "index.hpp"

namespace ioremap {
namespace thevoid {

class elliptics_base
{
public:
	elliptics_base();

	bool initialize(const rapidjson::Value &config, const swarm::logger &logger);

	ioremap::elliptics::node node() const;
	ioremap::elliptics::session session() const;
	virtual bool process(const swarm::http_request &request, ioremap::elliptics::key &key, ioremap::elliptics::session &session) const;

protected:
	virtual bool prepare_config(const rapidjson::Value &config, dnet_config &node_config);
	virtual bool prepare_node(const rapidjson::Value &config, ioremap::elliptics::node &node);
	virtual bool prepare_session(const rapidjson::Value &config, ioremap::elliptics::session &session);

	swarm::logger logger() const;

private:
	swarm::logger m_logger;
	std::unique_ptr<ioremap::elliptics::node> m_node;
	std::unique_ptr<ioremap::elliptics::session> m_session;
};

}} // namespace ioremap::thevoid

#endif /*__IOREMAP_THEVOID_ELLIPTICS_SERVER_HPP */
