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
#include "elliptics_cache.hpp"
#include "signature_base.hpp"

using namespace ioremap::thevoid;

class example_server : public server<example_server>
{
public:
	struct signature_info
	{
		std::string key;
		std::string path;
	};

	example_server() : m_elliptics(this)
	{
	}

	~example_server()
	{
		if (m_cache) {
			m_cache->stop();
			m_cache.reset();
		}
	}

	virtual bool initialize(const rapidjson::Value &config)
	{
		if (!m_elliptics.initialize(config, logger()))
			return false;

		if (config.HasMember("signatures")) {
			auto &signatures = config["signatures"];
			for (auto it = signatures.Begin(); it != signatures.End(); ++it) {
				if (!it->HasMember("key")) {
					logger().log(ioremap::swarm::LOG_ERROR, "\"signatures[i].key\" field is missed");
					return false;
				}
				if (!it->HasMember("path")) {
					logger().log(ioremap::swarm::LOG_ERROR, "\"signatures[i].path\" field is missed");
					return false;
				}

				signature_info info = {
					(*it)["key"].GetString(),
					(*it)["path"].GetString()
				};

				m_signatures.emplace_back(std::move(info));
			}
		}

		if (config.HasMember("cache")) {
			m_cache = std::make_shared<elliptics::elliptics_cache>();
			m_cache->initialize(config, m_elliptics.node(), logger());
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

		on<elliptics::index::on_update<example_server>>(
			options::exact_match("/update"),
			options::methods("POST")
		);
		on<elliptics::index::on_find<example_server>>(
			options::exact_match("/find"),
			options::methods("GET")
		);
		if (m_redirect_read) {
			on<elliptics::io::on_redirectable_get<example_server>>(
				options::exact_match("/get"),
				options::methods("GET")
			);
		} else {
			on<elliptics::io::on_get<example_server>>(
				options::exact_match("/get"),
				options::methods("GET")
			);
		}
		on<elliptics::io::on_buffered_get<example_server>>(
			options::exact_match("/get-big"),
			options::methods("GET")
		);
		on<elliptics::io::on_upload<example_server>>(
			options::exact_match("/upload"),
			options::methods("POST")
		);
		on<elliptics::io::on_buffered_upload<example_server>>(
			options::exact_match("/upload-big"),
			options::methods("POST")
		);
		on<elliptics::io::on_download_info<example_server>>(
			options::exact_match("/download-info"),
			options::methods("GET")
		);
		on<elliptics::common::on_ping<example_server>>(
			options::exact_match("/ping"),
			options::methods("GET")
		);
		on<elliptics::common::on_echo<example_server>>(
			options::exact_match("/echo"),
			options::methods("GET")
		);
	
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

	ioremap::swarm::url generate_url_base(dnet_addr *addr)
	{
		char buffer[128];

		ioremap::swarm::url url;
		url.set_scheme(m_secured_http ? "https" : "http");
		url.set_host(dnet_server_convert_dnet_addr_raw(addr, buffer, sizeof(buffer)));
		if (m_redirect_port > 0) {
			url.set_port(m_redirect_port);
		}

		return std::move(url);
	}

	const elliptics_base *elliptics()
	{
		return &m_elliptics;
	}

	class elliptics_impl : public elliptics_base
	{
	public:
		elliptics_impl(example_server *server) : m_server(server)
		{
		}

		virtual bool process(const ioremap::swarm::http_request &request, ioremap::elliptics::key &key, ioremap::elliptics::session &session) const
		{
			if (!elliptics_base::process(request, key, session)) {
				return false;
			}

			if (m_server->m_cache) {
				auto cache_groups = m_server->m_cache->groups(key);
				if (!cache_groups.empty()) {
					auto groups = session.get_groups();
					groups.insert(groups.end(), cache_groups.begin(), cache_groups.end());
					session.set_groups(groups);
				}
			}

			return true;
		}

	private:
		example_server *m_server;
	};

private:
	std::vector<signature_info> m_signatures;
	bool m_redirect_read;
	int m_redirect_port;
	bool m_secured_http;
	std::shared_ptr<elliptics::elliptics_cache> m_cache;
	elliptics_impl m_elliptics;
	elliptics::signature_base m_signature;
};

int main(int argc, char **argv)
{
	return run_server<example_server>(argc, argv);
}
