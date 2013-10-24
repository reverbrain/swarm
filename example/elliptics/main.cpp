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

#include "server.hpp"

using namespace ioremap::thevoid;

class example_server : public server<example_server>, public elliptics_server
{
public:
	struct signature_info
	{
		std::string key;
		std::string path;
	};

	virtual bool initialize(const rapidjson::Value &config) {
		if (!elliptics_server::initialize(config))
			return false;

		set_logger(get_logger_impl());

		if (config.HasMember("signatures")) {
			auto &signatures = config["signatures"];
			for (auto it = signatures.Begin(); it != signatures.End(); ++it) {
				if (!it->HasMember("key")) {
					get_logger().log(ioremap::swarm::LOG_ERROR, "\"signatures[i].key\" field is missed");
					return false;
				}
				if (!it->HasMember("path")) {
					get_logger().log(ioremap::swarm::LOG_ERROR, "\"signatures[i].path\" field is missed");
					return false;
				}

				signature_info info = {
					(*it)["key"].GetString(),
					(*it)["path"].GetString()
				};

				m_signatures.emplace_back(std::move(info));
			}
		}

		if (config.HasMember("redirect")) {
			m_redirect_read = config["redirect"].GetBool();
		} else {
			m_redirect_read = false;
		}

		if (config.HasMember("redirect-port")) {
			m_redirect_port = config["redirect-port"].GetInt();
		} else {
			m_redirect_port = -1;
		}

		if (config.HasMember("https")) {
			m_secured_http = config["https"].GetBool();
		} else {
			m_secured_http = false;
		}

		on<elliptics::index::on_update<example_server>>("/update");
		on<elliptics::index::on_find<example_server>>("/find");
		if (m_redirect_read) {
			on<elliptics::io::on_redirectable_get<example_server>>("/get");
		} else {
			on<elliptics::io::on_get<example_server>>("/get");
		}
		on<elliptics::io::on_upload<example_server>>("/upload");
		on<elliptics::io::on_download_info<example_server>>("/download-info");
		on<elliptics::common::on_ping<example_server>>("/ping");
		on<elliptics::common::on_echo<example_server>>("/echo");
	
		return true;
	}

	const std::string *find_signature(const std::string &path)
	{
		for (auto it = m_signatures.begin(); it != m_signatures.end(); ++it) {
			if (it->path.size() <= path.size()
				&& path.compare(0, it->path.size(), it->path) == 0) {
				return &it->key;
			}
		}

		return NULL;
	}

	std::string generate_url_base(dnet_addr *addr)
	{
		std::string url = m_secured_http ? "https://" : "http://";
		url += dnet_state_dump_addr_only(addr);
		if (m_redirect_port > 0) {
			url += ":";
			url += boost::lexical_cast<std::string>(m_redirect_port);
		}
		url += "/";
		return url;
	}

private:
	std::vector<signature_info> m_signatures;
	bool m_redirect_read;
	int m_redirect_port;
	bool m_secured_http;
};

int main(int argc, char **argv)
{
	return run_server<example_server>(argc, argv);
}
