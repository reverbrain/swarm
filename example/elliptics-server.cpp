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

#include <swarm/network_url.h>
#include <swarm/network_query_list.h>

#include <thevoid/rapidjson/stringbuffer.h>
#include <thevoid/rapidjson/prettywriter.h>

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

	on<on_update>("/update");
	on<on_find>("/find");
	on<on_get>("/get");
	on<on_upload>("/upload");
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

	sess.set_indexes(id, indexes_entries)
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

	rapidjson::Document data;
	data.Parse<0>(boost::asio::buffer_cast<const char*>(buffer));

	if (data.HasParseError()) {
		get_reply()->send_error(swarm::network_reply::bad_request);
		return;
	}

	if (!data.HasMember("type") || !data.HasMember("indexes")) {
		get_reply()->send_error(swarm::network_reply::bad_request);
		return;
	}

	session sess = get_server()->create_session();

	const std::string type = data["type"].GetString();

	auto &indexesArray = data["indexes"];

	std::vector<dnet_raw_id> indexes;

	for (auto it = indexesArray.Begin(); it != indexesArray.End(); ++it) {
		key index = std::string(it->GetString());
		sess.transform(index);

		indexes.push_back(index.raw_id());
		m_map[index.raw_id()] = index.to_string();
	}

	if (type != "and" && type != "or") {
		get_reply()->send_error(swarm::network_reply::bad_request);
		return;
	}

	(type == "and" ? sess.find_all_indexes(indexes) : sess.find_any_indexes(indexes))
			.connect(std::bind(&on_find::on_find_finished, shared_from_this(), _1, _2));
}

class JsonValue : public rapidjson::Value
{
public:
	JsonValue()
	{
		SetObject();
	}

	~JsonValue()
	{
	}

	std::string ToString()
	{
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

		Accept(writer);
		buffer.Put('\n');

		return std::string(buffer.GetString(), buffer.GetSize());
	}

	rapidjson::MemoryPoolAllocator<> &GetAllocator()
	{
		return m_allocator;
	}

private:
	rapidjson::MemoryPoolAllocator<> m_allocator;
};

void elliptics_server::on_find::on_find_finished(const sync_find_indexes_result &result, const error_info &error)
{
	if (error) {
		get_reply()->send_error(swarm::network_reply::service_unavailable);
		return;
	}

	JsonValue result_object;

	static char id_str[2 * DNET_ID_SIZE + 1];

	for (size_t i = 0; i < result.size(); ++i) {
		const find_indexes_result_entry &entry = result[i];

		rapidjson::Value indexes;
		indexes.SetObject();

		for (auto it = entry.indexes.begin(); it != entry.indexes.end(); ++it) {
			const std::string data = it->data.to_string();
			rapidjson::Value value(data.c_str(), data.size(), result_object.GetAllocator());
			indexes.AddMember(m_map[it->index].c_str(), value, result_object.GetAllocator());
		}

		dnet_dump_id_len_raw(entry.id.id, DNET_ID_SIZE, id_str);
		result_object.AddMember(id_str, indexes, result_object.GetAllocator());
	}

	const std::string result_str = result_object.ToString();

	swarm::network_reply reply;
	reply.set_code(swarm::network_reply::ok);
	reply.set_content_length(result_str.size());
	reply.set_content_type("text/json");
	get_reply()->send_headers(reply, boost::asio::buffer(result_str),
				  std::bind(&on_find::on_send_finished, shared_from_this(), result_str));
}

void elliptics_server::on_find::on_send_finished(const std::string &)
{
	get_reply()->close(boost::system::error_code());
}

void elliptics_server::on_find::on_close(const boost::system::error_code &err)
{
	(void) err;
}

void elliptics_server::on_get::on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	using namespace std::placeholders;

	swarm::network_url url(req.get_url());
	swarm::network_query_list query_list(url.query());

	if (!query_list.has_item("name") && !query_list.has_item("id")) {
		get_reply()->send_error(network_reply::bad_request);
		return;
	}

	session sess = get_server()->create_session();

	if (query_list.has_item("name")) {
		std::string name = query_list.item_value("name");
		sess.read_data(name, 0, 0).connect(std::bind(&on_get::on_read_finished, shared_from_this(), _1, _2));
	} else {
		std::string sid = query_list.item_value("id");

		struct dnet_id id;
		memset(&id, 0, sizeof(struct dnet_id));

		dnet_parse_numeric_id(sid.c_str(), id.id);
		sess.read_data(id, 0, 0).connect(std::bind(&on_get::on_read_finished, shared_from_this(), _1, _2));
	}
}

void elliptics_server::on_get::on_read_finished(const sync_read_result &result, const error_info &error)
{
	if (error.code() == -ENOENT) {
		get_reply()->send_error(swarm::network_reply::not_found);
		return;
	} else if (error) {
		get_reply()->send_error(swarm::network_reply::service_unavailable);
		return;
	}

	const read_result_entry &entry = result[0];

	data_pointer file = entry.file();

	const dnet_time &ts = entry.io_attribute()->timestamp;
	const network_request &request = get_request();

	if (request.has_if_modified_since()) {
		if (ts.tsec <= request.get_if_modified_since()) {
			get_reply()->send_error(swarm::network_reply::not_modified);
			return;
		}
	}

	swarm::network_reply reply;
	reply.set_code(swarm::network_reply::ok);
	reply.set_content_length(file.size());
	reply.set_content_type("text/plain");
	reply.set_last_modified(ts.tsec);
	get_reply()->send_headers(reply, boost::asio::buffer(file.data(), file.size()),
				  std::bind(&on_get::on_send_finished, shared_from_this(), file));
}

void elliptics_server::on_get::on_send_finished(const data_pointer &)
{
	get_reply()->close(boost::system::error_code());
}

void elliptics_server::on_get::on_close(const boost::system::error_code &err)
{
	(void) err;
}


void elliptics_server::on_upload::on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	using namespace std::placeholders;

	swarm::network_url url(req.get_url());
	swarm::network_query_list query_list(url.query());

	std::string name = query_list.item_value("name");

	session sess = get_server()->create_session();

	auto data = boost::asio::buffer_cast<const char*>(buffer);
	auto size = boost::asio::buffer_size(buffer);

	sess.write_data(name, data_pointer::from_raw(const_cast<char *>(data), size), 0)
			.connect(std::bind(&on_upload::on_write_finished, shared_from_this(), _1, _2));
}

void elliptics_server::on_upload::on_write_finished(const sync_write_result &result, const error_info &error)
{
	if (error) {
		get_reply()->send_error(swarm::network_reply::service_unavailable);
		return;
	}

	const write_result_entry &entry = result[0];

	swarm::network_reply reply;
	reply.set_code(swarm::network_reply::ok);
	reply.set_content_length(0);
	reply.set_content_type("text/html");
	get_reply()->send_headers(reply, boost::asio::buffer("", 0),
				  std::bind(&reply_stream::close, get_reply(), std::placeholders::_1));
}

void elliptics_server::on_upload::on_close(const boost::system::error_code &err)
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
	return run_server<elliptics_server>(argc, argv);
}
