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

#include "signature_base.hpp"

namespace ioremap {
namespace thevoid {
namespace elliptics {

signature_base::signature_base()
{
}

bool signature_base::initialize(const rapidjson::Value &config, const ioremap::elliptics::node &node, const swarm::logger &logger)
{
	m_logger = logger;

	if (!config.HasMember("signature")) {
		m_logger.log(swarm::LOG_ERROR, "\"signature\" field is missed");
		return false;
	}

	const rapidjson::Value &signature_config = config["signature"];
	if (!signature_config.HasMember("key")) {
		m_logger.log(swarm::LOG_ERROR, "\"signature.key\" field is missed");
		return false;
	}

	m_node.reset(new ioremap::elliptics::node(node));
	m_key = signature_config["key"].GetString();

	return true;
}

std::string signature_base::sign(const swarm::url &url) const
{
	const swarm::url_query &query = url.query();

	std::vector<std::pair<std::string, std::string>> items;
	for (size_t i = 0; i < query.count(); ++i) {
		items.emplace_back(query.item(i));
	}
	items.emplace_back("key", m_key);

	std::sort(items.begin(), items.end());

	swarm::url_query new_query;
	for (auto it = items.begin(); it != items.end(); ++it) {
		new_query.add_item(it->first, it->second);
	}

	swarm::url new_url = url;
	new_url.set_query(new_query);

	std::string result = new_url.to_string();

	dnet_raw_id signature_id;
	dnet_transform_node(m_node->get_native(),
		result.c_str(), result.size(),
		signature_id.id, sizeof(signature_id.id));

	char signature_str[2 * DNET_ID_SIZE + 1];
	dnet_dump_id_len_raw(signature_id.id, DNET_ID_SIZE, signature_str);

	return std::string(signature_str, 2 * DNET_ID_SIZE);
}

swarm::logger signature_base::logger() const
{
	return m_logger;
}

} // namespace elliptics
} // namespace thevoid
} // namespace ioremap
