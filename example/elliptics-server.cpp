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

#include "elliptics-server.hpp"
#include <iostream>

using namespace ioremap::thevoid;
using namespace ioremap::elliptics;

elliptics_server::elliptics_server()
{
}

bool elliptics_server::initialize(const rapidjson::Value &config)
{
	if (!config.HasMember("remotes")) {
		std::cerr << "\"remotes\" field is missed";
		return false;
	}

	if (!config.HasMember("groups")) {
		std::cerr << "\"groups\" field is missed";
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

	on<on_update>("/update");
	on<on_find>("/find");
	on<on_ping>("/ping");

	return true;
}

session elliptics_server::create_session()
{
	return m_session->clone();
}

void elliptics_server::on_update::on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	using namespace std::placeholders;

	(void) req;

	rapidjson::Document doc;
	doc.Parse<0>(boost::asio::buffer_cast<const char*>(buffer));

	if (doc.HasParseError()) {
		get_reply()->send_error(swarm::network_reply::bad_request);
		return;
	}

	if (!doc.HasMember("id") || !doc.HasMember("indexes")) {
		get_reply()->send_error(swarm::network_reply::bad_request);
		return;
	}

	session sess = get_server()->create_session();

	std::string id = doc["id"].GetString();
	std::vector<index_entry> indexes_entries;

	index_entry entry;

	auto &indexes = doc["indexes"];
	for (auto it = indexes.MemberBegin(); it != indexes.MemberEnd(); ++it) {
		sess.transform(it->name.GetString(), entry.index);
		entry.data = data_pointer::copy(it->value.GetString(), it->value.GetStringLength());

		indexes_entries.push_back(entry);
	}

	sess.update_indexes(id, indexes_entries)
			.connect(std::bind(&on_update::on_update_finished, shared_from_this(), _2));
}

void elliptics_server::on_update::on_update_finished(const error_info &error)
{
	if (error) {
		get_reply()->send_error(swarm::network_reply::service_unavailable);
		return;
	}

	get_reply()->send_error(swarm::network_reply::ok);
}

void elliptics_server::on_update::on_close(const boost::system::error_code &err)
{
	std::cerr << "closed" << std::endl;
}

void elliptics_server::on_find::on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	using namespace std::placeholders;

	(void) req;


	get_reply()->send_error(swarm::network_reply::not_implemented);

	rapidjson::Document doc;
	doc.Parse<0>(boost::asio::buffer_cast<const char*>(buffer));

	if (doc.HasParseError()) {
		get_reply()->send_error(swarm::network_reply::bad_request);
		return;
	}

	if (!doc.HasMember("id") || !doc.HasMember("indexes")) {
		get_reply()->send_error(swarm::network_reply::bad_request);
		return;
	}

	session sess = get_server()->create_session();

	auto &indexesArray = doc["indexes"];

	std::vector<std::string> indexes;

	std::transform(indexesArray.Begin(), indexesArray.End(),
		std::back_inserter(indexes),
		std::bind(&rapidjson::Value::GetString, std::placeholders::_1));

	sess.find_indexes(indexes)
			.connect(std::bind(&on_find::on_find_finished, shared_from_this(), _1, _2));
}

void elliptics_server::on_find::on_find_finished(const sync_find_indexes_result &result, const error_info &error)
{
	if (error) {
		get_reply()->send_error(swarm::network_reply::service_unavailable);
		return;
	}

	rapidjson::Document doc;

	for (size_t i = 0; i < result.size(); ++i) {
		const find_indexes_result_entry &entry = result[i];

//		entry.id
	}
}

void elliptics_server::on_find::on_close(const boost::system::error_code &err)
{
	(void) err;
}

void elliptics_server::on_ping::on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	auto conn = get_reply();

	swarm::network_reply reply;
	reply.set_code(swarm::network_reply::ok);
	reply.set_content_length(0);
	reply.set_content_type("text/html");

	conn->send_headers(reply, boost::asio::buffer("", 0),
			   std::bind(&reply_stream::close, conn, std::placeholders::_1));
}

void elliptics_server::on_ping::on_close(const boost::system::error_code &err)
{
	(void) err;
}


int main(int argc, char **argv)
{
    return create_server<elliptics_server>()->run(argc, argv);
}
