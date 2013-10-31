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

#include <elliptics/utils.hpp>
#include "elliptics_logger.hpp"

#include <iostream>

#include "server.hpp"

using namespace ioremap::swarm;
using namespace ioremap::thevoid;
using namespace ioremap::elliptics;

elliptics_base::elliptics_base()
{
}

bool elliptics_base::initialize(const rapidjson::Value &config, const swarm::logger &logger)
{
	m_logger = logger;

	dnet_config node_config;
	memset(&node_config, 0, sizeof(node_config));

	if (!prepare_config(config, node_config)) {
		return false;
	}

	m_node.reset(new ioremap::elliptics::node(swarm_logger(logger), node_config));

	if (!prepare_node(config, *m_node)) {
		return false;
	}

	m_session.reset(new ioremap::elliptics::session(*m_node));

	if (!prepare_session(config, *m_session)) {
		return false;
	}

	return true;
}

node elliptics_base::node() const
{
	return *m_node;
}

session elliptics_base::session() const
{
	return m_session->clone();
}

bool elliptics_base::process(const swarm::http_request &request, ioremap::elliptics::key &key, ioremap::elliptics::session &session) const
{
	const auto &query = request.url().query();

	if (auto name = query.item_value("name")) {
		key = *name;
	} else if (auto sid = query.item_value("id")) {
		struct dnet_id id;
		memset(&id, 0, sizeof(struct dnet_id));

		dnet_parse_numeric_id(sid->c_str(), id.id);

		key = id;
	} else {
		return false;
	}

	session.transform(key);

	(void) session;

	return true;
}

bool elliptics_base::prepare_config(const rapidjson::Value &config, dnet_config &node_config)
{
	(void) config;
	(void) node_config;
	return true;
}

bool elliptics_base::prepare_node(const rapidjson::Value &config, ioremap::elliptics::node &node)
{
	if (!config.HasMember("remotes")) {
		m_logger.log(swarm::LOG_ERROR, "\"remotes\" field is missed");
		return false;
	}

	std::vector<std::string> remotes;

	auto &remotesArray = config["remotes"];
	std::transform(remotesArray.Begin(), remotesArray.End(),
		std::back_inserter(remotes),
		std::bind(&rapidjson::Value::GetString, std::placeholders::_1));

	for (auto it = remotes.begin(); it != remotes.end(); ++it) {
		node.add_remote(it->c_str());
	}

	return true;
}

bool elliptics_base::prepare_session(const rapidjson::Value &config, ioremap::elliptics::session &session)
{
	if (!config.HasMember("groups")) {
		m_logger.log(swarm::LOG_ERROR, "\"groups\" field is missed");
		return false;
	}

	std::vector<int> groups;

	auto &groupsArray = config["groups"];
	std::transform(groupsArray.Begin(), groupsArray.End(),
		std::back_inserter(groups),
		std::bind(&rapidjson::Value::GetInt, std::placeholders::_1));

	session.set_groups(groups);

	return true;
}

ioremap::swarm::logger elliptics_base::logger() const
{
	return m_logger;
}
