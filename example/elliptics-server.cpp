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

#include <boost/asio.hpp>
#include <elliptics/utils.hpp>

namespace {
	static std::string lexical_cast(size_t value) {
		if (value == 0) {
			return std::string("0");
		}

		std::string result;
		size_t length = 0;
		size_t calculated = value;
		while (calculated) {
			calculated /= 10;
			++length;
		}

		result.resize(length);
		while (value) {
			--length;
			result[length] = '0' + (value % 10);
			value /= 10;
		}

		return result;
	}
}

namespace boost { namespace asio {

const_buffer buffer(const ioremap::elliptics::data_pointer &data)
{
	return buffer(data.data(), data.size());
}

} }

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
	on<on_echo>("/echo");

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
		send_reply(swarm::network_reply::bad_request);
		return;
	}

	if (!doc.HasMember("id") || !doc.HasMember("indexes")) {
		send_reply(swarm::network_reply::bad_request);
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
		send_reply(swarm::network_reply::service_unavailable);
		return;
	}

	send_reply(swarm::network_reply::ok);
}

void elliptics_server::on_find::on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	using namespace std::placeholders;

	(void) req;

	rapidjson::Document data;
	data.Parse<0>(boost::asio::buffer_cast<const char*>(buffer));

	if (data.HasParseError()) {
		send_reply(swarm::network_reply::bad_request);
		return;
	}

	if (!data.HasMember("type") || !data.HasMember("indexes")) {
		send_reply(swarm::network_reply::bad_request);
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
		send_reply(swarm::network_reply::bad_request);
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
		send_reply(swarm::network_reply::service_unavailable);
		return;
	}

	JsonValue result_object;

	char id_str[2 * DNET_ID_SIZE + 1];

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

	swarm::network_reply reply;
	reply.set_code(swarm::network_reply::ok);
	reply.set_content_type("text/json");
	reply.set_data(result_object.ToString());
	reply.set_content_length(reply.get_data().size());

	send_reply(reply);
}

void elliptics_server::on_get::on_request(const ioremap::swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	using namespace std::placeholders;

	swarm::network_url url(req.get_url());
	swarm::network_query_list query_list(url.query());

	session sess = get_server()->create_session();

	if (auto name = query_list.try_item("name")) {
		sess.read_data(*name, 0, 0).connect(std::bind(&on_get::on_read_finished, shared_from_this(), _1, _2));
	} else if (auto sid = query_list.try_item("id")) {
		struct dnet_id id;
		memset(&id, 0, sizeof(struct dnet_id));

		dnet_parse_numeric_id(sid->c_str(), id.id);
		sess.read_data(id, 0, 0).connect(std::bind(&on_get::on_read_finished, shared_from_this(), _1, _2));
	} else {
		send_reply(network_reply::bad_request);
	}
}

void elliptics_server::on_get::on_read_finished(const sync_read_result &result, const error_info &error)
{
	if (error.code() == -ENOENT) {
		send_reply(swarm::network_reply::not_found);
		return;
	} else if (error) {
		send_reply(swarm::network_reply::service_unavailable);
		return;
	}

	const read_result_entry &entry = result[0];

	data_pointer file = entry.file();

	const dnet_time &ts = entry.io_attribute()->timestamp;
	const network_request &request = get_request();

	if (request.has_if_modified_since()) {
		if (ts.tsec <= request.get_if_modified_since()) {
			send_reply(swarm::network_reply::not_modified);
			return;
		}
	}

	swarm::network_reply reply;
	reply.set_code(swarm::network_reply::ok);
	reply.set_content_length(file.size());
	reply.set_content_type("text/plain");
	reply.set_last_modified(ts.tsec);

	send_reply(reply, std::move(file));
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
		send_reply(swarm::network_reply::service_unavailable);
		return;
	}

	const write_result_entry &entry = result[0];

	JsonValue result_object;

	char id_str[2 * DNET_ID_SIZE + 1];
	dnet_dump_id_len_raw(entry.command()->id.id, DNET_ID_SIZE, id_str);
	rapidjson::Value id_str_value(id_str, 2 * DNET_ID_SIZE, result_object.GetAllocator());
	result_object.AddMember("id", id_str_value, result_object.GetAllocator());

	char csum_str[2 * DNET_ID_SIZE + 1];
	dnet_dump_id_len_raw(entry.file_info()->checksum, DNET_ID_SIZE, csum_str);
	rapidjson::Value csum_str_value(csum_str, 2 * DNET_ID_SIZE, result_object.GetAllocator());
	result_object.AddMember("csum", csum_str_value, result_object.GetAllocator());

	if (entry.file_path())
		result_object.AddMember("filename", entry.file_path(), result_object.GetAllocator());

	result_object.AddMember("size", entry.file_info()->size, result_object.GetAllocator());
	result_object.AddMember("offset-within-data-file", entry.file_info()->offset, result_object.GetAllocator());

	char str[64];
	struct tm tm;
	struct timeval tv;

	localtime_r((time_t *)&entry.file_info()->mtime.tsec, &tm);
	strftime(str, sizeof(str), "%F %Z %R:%S", &tm);

	char time_str[128];
	snprintf(time_str, sizeof(time_str), "%s.%06lu", str, entry.file_info()->mtime.tnsec / 1000);

	result_object.AddMember("mtime", time_str, result_object.GetAllocator());
	std::string raw_time = lexical_cast(entry.file_info()->mtime.tsec) + "." + lexical_cast(entry.file_info()->mtime.tnsec / 1000);
	result_object.AddMember("mtime-raw", raw_time.c_str(), result_object.GetAllocator());
	
	char addr_str[128];
	result_object.AddMember("server", dnet_server_convert_dnet_addr_raw(entry.storage_address(), addr_str, sizeof(addr_str)),
				result_object.GetAllocator());

	swarm::network_reply reply;
	reply.set_code(swarm::network_reply::ok);
	reply.set_content_type("text/json");
	reply.set_data(result_object.ToString());
	reply.set_content_length(reply.get_data().size());

	send_reply(reply);
}

void elliptics_server::on_ping::on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer)
{
	send_reply(swarm::network_reply::ok);
}

void elliptics_server::on_echo::on_request(const network_request &req, const boost::asio::const_buffer &buffer)
{
	auto data = boost::asio::buffer_cast<const char*>(buffer);
	auto size = boost::asio::buffer_size(buffer);

	swarm::network_reply reply;
	reply.set_code(swarm::network_reply::ok);
	reply.set_headers(req.get_headers());
	reply.set_content_length(size);

	send_reply(reply, std::string(data, size));
}

int main(int argc, char **argv)
{
	return run_server<elliptics_server>(argc, argv);
}
