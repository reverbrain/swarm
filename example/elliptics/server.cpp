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

#include <iostream>

#include "server.hpp"

using namespace ioremap::swarm;
using namespace ioremap::thevoid;
using namespace ioremap::elliptics;

elliptics_server::elliptics_server()
{
}

bool elliptics_server::initialize(const rapidjson::Value &config)
{
	if (!config.HasMember("remotes")) {
		std::cerr << "\"remotes\" field is missed" << std::endl;
		return false;
	}

	if (!config.HasMember("groups")) {
		std::cerr << "\"groups\" field is missed" << std::endl;
		return false;
	}

	std::vector<std::string> remotes;
	std::vector<int> groups;
	std::string logfile = "/dev/stderr";
	int loglevel = DNET_LOG_INFO;

	if (config.HasMember("logfile"))
		logfile = config["logfile"].GetString();

	if (config.HasMember("loglevel"))
		loglevel = config["loglevel"].GetInt();

	m_logger.reset(new file_logger(logfile.c_str(), loglevel));
	m_node.reset(new node(*m_logger));
	m_session.reset(new session(*m_node));

	auto &remotesArray = config["remotes"];
	std::transform(remotesArray.Begin(), remotesArray.End(),
		std::back_inserter(remotes),
		std::bind(&rapidjson::Value::GetString, std::placeholders::_1));

	for (auto it = remotes.begin(); it != remotes.end(); ++it) {
		m_node->add_remote(it->c_str());
	}

	auto &groupsArray = config["groups"];
	std::transform(groupsArray.Begin(), groupsArray.End(),
		std::back_inserter(groups),
		std::bind(&rapidjson::Value::GetInt, std::placeholders::_1));

	m_session->set_groups(groups);

	return true;
}

session elliptics_server::create_session()
{
	return m_session->clone();
}
