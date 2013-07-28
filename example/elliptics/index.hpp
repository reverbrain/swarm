/*
 * 2013+ Copyright (c) Ruslan Nigatullin <euroelessar@yandex.ru>
 * 2013+ Copyright (c) Evgeniy Polyakov <zbr@ioremap.net>
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

#ifndef __IOREMAP_THEVOID_ELLIPTICS_INDEX_HPP
#define __IOREMAP_THEVOID_ELLIPTICS_INDEX_HPP

#include <thevoid/server.hpp>

#include "asio.hpp"
#include "jsonvalue.hpp"

namespace ioremap { namespace thevoid { namespace elliptics { namespace index { 

struct id_comparator
{
	bool operator() (const dnet_raw_id &first, const dnet_raw_id &second) const
	{
		return memcmp(first.id, second.id, sizeof(first.id)) < 0;
	}
};

// set indexes for given ID
template <typename T>
struct on_update : public simple_request_stream<T>, public std::enable_shared_from_this<on_update<T>>
{
	virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) {
		(void) req;

		rapidjson::Document doc;
		doc.Parse<0>(boost::asio::buffer_cast<const char*>(buffer));

		if (doc.HasParseError()) {
			this->send_reply(swarm::network_reply::bad_request);
			return;
		}

		if (!doc.HasMember("id") || !doc.HasMember("indexes")) {
			this->send_reply(swarm::network_reply::bad_request);
			return;
		}

		ioremap::elliptics::session sess = this->get_server()->create_session();

		std::string id = doc["id"].GetString();
		std::vector<ioremap::elliptics::index_entry> indexes_entries;

		ioremap::elliptics::index_entry entry;

		auto &indexes = doc["indexes"];
		for (auto it = indexes.MemberBegin(); it != indexes.MemberEnd(); ++it) {
			sess.transform(it->name.GetString(), entry.index);
			entry.data = ioremap::elliptics::data_pointer::copy(it->value.GetString(),
					it->value.GetStringLength());

			indexes_entries.push_back(entry);
		}

		sess.set_indexes(id, indexes_entries)
				.connect(std::bind(&on_update::on_update_finished,
							this->shared_from_this(), std::placeholders::_2));
	}

	virtual void on_update_finished(const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::network_reply::service_unavailable);
			return;
		}

		this->send_reply(swarm::network_reply::ok);
	}
};

// find (using 'AND' or 'OR' operator) indexes, which contain given ID
template <typename T>
struct on_find : public simple_request_stream<T>, public std::enable_shared_from_this<on_find<T>>
{
	struct read_result_cmp {
		bool operator ()(const ioremap::elliptics::read_result_entry &e1,
				const ioremap::elliptics::read_result_entry &e2) const {
			return dnet_id_cmp(&e1.command()->id, &e2.command()->id) < 0;
		}

		const dnet_raw_id &operator()(const ioremap::elliptics::find_indexes_result_entry &e) const {
			return e.id;
		}

		bool operator ()(const ioremap::elliptics::read_result_entry &e1, const dnet_raw_id &id) const {
			return dnet_id_cmp_str((const unsigned char *)e1.command()->id.id, id.id) < 0;
		}
	} m_rrcmp;

	virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) {
		(void) req;

		rapidjson::Document data;
		data.Parse<0>(boost::asio::buffer_cast<const char*>(buffer));

		if (data.HasParseError()) {
			this->send_reply(swarm::network_reply::bad_request);
			return;
		}

		if (!data.HasMember("type") || !data.HasMember("indexes")) {
			this->send_reply(swarm::network_reply::bad_request);
			return;
		}

		m_view = "id-only";
		if (data.HasMember("view"))
			m_view = data["view"].GetString();

		ioremap::elliptics::session sess = this->get_server()->create_session();

		const std::string type = data["type"].GetString();

		auto &indexesArray = data["indexes"];

		std::vector<dnet_raw_id> indexes;

		for (auto it = indexesArray.Begin(); it != indexesArray.End(); ++it) {
			ioremap::elliptics::key index = std::string(it->GetString());
			sess.transform(index);

			indexes.push_back(index.raw_id());
			m_map[index.raw_id()] = index.to_string();
		}

		if (type != "and" && type != "or") {
			this->send_reply(swarm::network_reply::bad_request);
			return;
		}

		(type == "and" ? sess.find_all_indexes(indexes) : sess.find_any_indexes(indexes))
				.connect(std::bind(&on_find::on_find_finished,
					this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
	}

	virtual void on_find_finished(const ioremap::elliptics::sync_find_indexes_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::network_reply::service_unavailable);
			return;
		}

		if (m_view == "extended") {
			std::vector<ioremap::elliptics::key> ids;
			std::transform(result.begin(), result.end(), std::back_inserter(ids), m_rrcmp);

			m_result = result;

			ioremap::elliptics::session sess = this->get_server()->create_session();
			sess.bulk_read(ids).connect(std::bind(&on_find::on_ready_to_parse_indexes,
					this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
		} else {
			ioremap::elliptics::sync_read_result data;
			send_indexes_reply(data, result);
		}
	}

	virtual void on_ready_to_parse_indexes(const ioremap::elliptics::sync_read_result &data,
			const ioremap::elliptics::error_info &error) {
		ioremap::elliptics::sync_read_result tmp;

		if (error) {
			send_indexes_reply(tmp, m_result);
		} else {
			tmp = data;
			std::sort(tmp.begin(), tmp.end(), m_rrcmp);
			send_indexes_reply(tmp, m_result);
		}
	}

	virtual void send_indexes_reply(ioremap::elliptics::sync_read_result &data,
			const ioremap::elliptics::sync_find_indexes_result &result) {
		JsonValue result_object;

		for (size_t i = 0; i < result.size(); ++i) {
			const ioremap::elliptics::find_indexes_result_entry &entry = result[i];

			rapidjson::Value val;
			val.SetObject();

			rapidjson::Value indexes;
			indexes.SetObject();

			for (auto it = entry.indexes.begin(); it != entry.indexes.end(); ++it) {
				const std::string data = it->data.to_string();
				rapidjson::Value value(data.c_str(), data.size(), result_object.GetAllocator());
				indexes.AddMember(m_map[it->index].c_str(), value, result_object.GetAllocator());
			}

			if (data.size()) {
				rapidjson::Value obj;
				obj.SetObject();

				auto it = std::lower_bound(data.begin(), data.end(), entry.id, m_rrcmp);
				if (it != data.end()) {
					rapidjson::Value data_str(reinterpret_cast<char *>(it->file().data()),
							it->file().size());
					obj.AddMember("data", data_str, result_object.GetAllocator());

					rapidjson::Value tobj;
					JsonValue::set_time(tobj, result_object.GetAllocator(),
							it->io_attribute()->timestamp.tsec,
							it->io_attribute()->timestamp.tnsec / 1000);
					obj.AddMember("mtime", tobj, result_object.GetAllocator());
				}

				val.AddMember("data-object", obj, result_object.GetAllocator());
			}

			val.AddMember("indexes", indexes, result_object.GetAllocator());

			char id_str[2 * DNET_ID_SIZE + 1];
			dnet_dump_id_len_raw(entry.id.id, DNET_ID_SIZE, id_str);
			result_object.AddMember(id_str, result_object.GetAllocator(),
					val, result_object.GetAllocator());
		}

		swarm::network_reply reply;
		reply.set_code(swarm::network_reply::ok);
		reply.set_content_type("text/json");
		reply.set_data(result_object.ToString());
		reply.set_content_length(reply.get_data().size());

		this->send_reply(reply);
	}

	std::map<dnet_raw_id, std::string, id_comparator> m_map;
	std::string m_view;
	ioremap::elliptics::sync_find_indexes_result m_result;
};

}}}} // namespace ioremap::thevoid::elliptics::index

#endif /*__IOREMAP_THEVOID_ELLIPTICS_INDEX_HPP */
