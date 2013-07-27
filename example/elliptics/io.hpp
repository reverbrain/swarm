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

#ifndef __IOREMAP_THEVOID_ELLIPTICS_IO_HPP
#define __IOREMAP_THEVOID_ELLIPTICS_IO_HPP

// must be the first, since thevoid internally uses X->boost::buffer conversion,
// which must be present at compile time
#include "asio.hpp"

#include <swarm/network_url.h>
#include <swarm/network_query_list.h>

#include <thevoid/server.hpp>

#include "jsonvalue.hpp"

namespace {
	static inline std::string lexical_cast(size_t value) {
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

namespace ioremap { namespace thevoid { namespace elliptics { namespace io {
// read data object
template <typename T>
struct on_get : public simple_request_stream<T>, public std::enable_shared_from_this<on_get<T>>
{
	virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) {
		using namespace std::placeholders;

		(void) buffer;

		swarm::network_url url(req.get_url());
		swarm::network_query_list query_list(url.query());

		ioremap::elliptics::session sess = this->get_server()->create_session();

		if (auto name = query_list.try_item("name")) {
			sess.read_data(*name, 0, 0).connect(
					std::bind(&on_get::on_read_finished, this->shared_from_this(), _1, _2));
		} else if (auto sid = query_list.try_item("id")) {
			struct dnet_id id;
			memset(&id, 0, sizeof(struct dnet_id));

			dnet_parse_numeric_id(sid->c_str(), id.id);
			sess.read_data(id, 0, 0).connect(
					std::bind(&on_get::on_read_finished, this->shared_from_this(), _1, _2));
		} else {
			this->send_reply(swarm::network_reply::bad_request);
		}
	}

	void on_read_finished(const ioremap::elliptics::sync_read_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error.code() == -ENOENT) {
			this->send_reply(swarm::network_reply::not_found);
			return;
		} else if (error) {
			this->send_reply(swarm::network_reply::service_unavailable);
			return;
		}

		const ioremap::elliptics::read_result_entry &entry = result[0];

		ioremap::elliptics::data_pointer file = entry.file();

		const dnet_time &ts = entry.io_attribute()->timestamp;
		const swarm::network_request &request = this->get_request();

		if (request.has_if_modified_since()) {
			if ((time_t)ts.tsec <= request.get_if_modified_since()) {
				this->send_reply(swarm::network_reply::not_modified);
				return;
			}
		}

		swarm::network_reply reply;
		reply.set_code(swarm::network_reply::ok);
		reply.set_content_length(file.size());
		reply.set_content_type("text/plain");
		reply.set_last_modified(ts.tsec);

		this->send_reply(reply, std::move(file));
	}
};

// write data object, get file-info json in response
template <typename T>
struct on_upload : public simple_request_stream<T>, public std::enable_shared_from_this<on_upload<T>>
{
	virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) {
		swarm::network_url url(req.get_url());
		swarm::network_query_list query_list(url.query());

		std::string name = query_list.item_value("name");

		ioremap::elliptics::session sess = this->get_server()->create_session();

		auto data = boost::asio::buffer_cast<const char*>(buffer);
		auto size = boost::asio::buffer_size(buffer);

		sess.write_data(name, ioremap::elliptics::data_pointer::from_raw(const_cast<char *>(data), size), 0)
				.connect(std::bind(&on_upload::on_write_finished, this->shared_from_this(),
							std::placeholders::_1, std::placeholders::_2));
	}

	void on_write_finished(const ioremap::elliptics::sync_write_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::network_reply::service_unavailable);
			return;
		}

		const ioremap::elliptics::write_result_entry &entry = result[0];

		elliptics::JsonValue result_object;

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
		result_object.AddMember("offset-within-data-file", entry.file_info()->offset,
				result_object.GetAllocator());

		char str[64];
		struct tm tm;

		localtime_r((time_t *)&entry.file_info()->mtime.tsec, &tm);
		strftime(str, sizeof(str), "%F %Z %R:%S", &tm);

		char time_str[128];
		snprintf(time_str, sizeof(time_str), "%s.%06lu", str, entry.file_info()->mtime.tnsec / 1000);

		result_object.AddMember("mtime", time_str, result_object.GetAllocator());
		std::string raw_time = lexical_cast(entry.file_info()->mtime.tsec) + "." +
			lexical_cast(entry.file_info()->mtime.tnsec / 1000);
		result_object.AddMember("mtime-raw", raw_time.c_str(), result_object.GetAllocator());
		
		char addr_str[128];
		result_object.AddMember("server",
			dnet_server_convert_dnet_addr_raw(entry.storage_address(), addr_str, sizeof(addr_str)),
				result_object.GetAllocator());

		swarm::network_reply reply;
		reply.set_code(swarm::network_reply::ok);
		reply.set_content_type("text/json");
		reply.set_data(result_object.ToString());
		reply.set_content_length(reply.get_data().size());

		this->send_reply(reply);
	}
};

}}}}	

#endif /*__IOREMAP_THEVOID_ELLIPTICS_IO_HPP */
