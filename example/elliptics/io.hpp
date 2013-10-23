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
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "jsonvalue.hpp"

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

		size_t offset = 0;
		size_t size = 0;

		try {
			if (auto tmp = query_list.try_item("offset"))
				offset = boost::lexical_cast<size_t>(*tmp);

			if (auto tmp = query_list.try_item("size"))
				size = boost::lexical_cast<size_t>(*tmp);
		} catch (std::exception &e) {
			this->log(swarm::LOG_ERROR, "GET request, invalid cast: %s", e.what());
			this->send_reply(swarm::network_reply::bad_request);
			return;
		}

		if (auto name = query_list.try_item("name")) {
			this->log(swarm::LOG_DEBUG, "GET request, name: \"%s\"", name->c_str());

			sess.read_data(*name, offset, size).connect(
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

	virtual void on_read_finished(const ioremap::elliptics::sync_read_result &result,
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

		if (auto tmp = request.try_header("Range")) {
			std::string range = *tmp;
			this->log(swarm::LOG_DATA, "GET, Range: \"%s\"", range.c_str());
			if (range.compare(0, 6, "bytes=") == 0) {
				range.erase(range.begin(), range.begin() + 6);
				std::vector<std::string> ranges;
				boost::split(ranges, range, boost::is_any_of(","));
				if (ranges.size() == 1)
					on_range(ranges[0], file, ts);
				else
					on_ranges(ranges, file, ts);
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

	bool parse_range(const std::string &range, size_t data_size, size_t &begin, size_t &end)
	{
		begin = 0;
		end = data_size - 1;

		if (range.size() <= 1)
			return false;

		try {
			const auto separator = range.find('-');
			if (separator == std::string::npos)
				return false;

			if (separator == 0) {
				auto tmp = boost::lexical_cast<size_t>(range.substr(separator + 1));
				if (tmp > data_size)
					begin = 0;
				else
					begin = data_size - tmp;
			} else {
				if (separator > 0)
					begin = boost::lexical_cast<size_t>(range.substr(0, separator));

				if (separator + 1 < range.size())
					end = boost::lexical_cast<size_t>(range.substr(separator + 1));
			}
		} catch (...) {
			return false;
		}

		if (begin > end)
			return false;

		if (begin >= data_size)
			return false;

		end = std::min(data_size - 1, end);

		return true;
	}

	std::string create_content_range(size_t begin, size_t end, size_t data_size)
	{
		std::string result = "bytes ";
		result += boost::lexical_cast<std::string>(begin);
		result += "-";
		result += boost::lexical_cast<std::string>(end);
		result += "/";
		result += boost::lexical_cast<std::string>(data_size);
		return result;
	}

	virtual void on_range(const std::string &range, const ioremap::elliptics::data_pointer &data, const dnet_time &ts)
	{
		size_t begin;
		size_t end;
		if (!parse_range(range, data.size(), begin, end)) {
			this->send_reply(swarm::network_reply::requested_range_not_satisfiable);
			return;
		}

		auto data_part = data.slice(begin, end + 1 - begin);

		swarm::network_reply reply;
		reply.set_code(swarm::network_reply::partial_content);
		reply.set_content_type("text/plain");
		reply.set_last_modified(ts.tsec);
		reply.add_header("Accept-Ranges", "bytes");
		reply.add_header("Content-Range", create_content_range(begin, end, data.size()));
		reply.set_content_length(data_part.size());

		this->send_reply(reply, std::move(data_part));
	}

	struct range_info
	{
		size_t begin;
		size_t end;
	};

	virtual void on_ranges(const std::vector<std::string> &ranges_str, const ioremap::elliptics::data_pointer &data, const dnet_time &ts)
	{
		std::vector<range_info> ranges;
		for (auto it = ranges_str.begin(); it != ranges_str.end(); ++it) {
			range_info info;
			if (parse_range(*it, data.size(), info.begin, info.end))
				ranges.push_back(info);
		}

		if (ranges.empty()) {
			this->send_reply(swarm::network_reply::requested_range_not_satisfiable);
			return;
		}

		char boundary[17];
		for (size_t i = 0; i < 2; ++i) {
			uint32_t tmp = rand();
			sprintf(boundary + i * 8, "%08X", tmp);
		}

		std::string result;
		for (auto it = ranges.begin(); it != ranges.end(); ++it) {
			result += "--";
			result += boundary;
			result += "\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Range: ";
			result += create_content_range(it->begin, it->end, data.size());
			result += "\r\n\r\n";
			result += data.slice(it->begin, it->end + 1 - it->begin).to_string();
			result += "\r\n";
		}
		result += "--";
		result += boundary;
		result += "--\r\n";

		swarm::network_reply reply;
		reply.set_code(swarm::network_reply::partial_content);
		reply.set_content_type(std::string("multipart/byteranges; boundary=") + boundary);
		reply.set_last_modified(ts.tsec);
		reply.add_header("Accept-Ranges", "bytes");
		reply.set_content_length(result.size());

		this->send_reply(reply, std::move(result));
	}
};

// write data object, get file-info json in response
template <typename T>
struct on_upload : public simple_request_stream<T>, public std::enable_shared_from_this<on_upload<T>>
{
	virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) {
		auto data = ioremap::elliptics::data_pointer::from_raw(
			const_cast<char *>(boost::asio::buffer_cast<const char*>(buffer)),
			boost::asio::buffer_size(buffer));

		try {
			write_data(req, data).connect(
				std::bind(&on_upload::on_write_finished, this->shared_from_this(),
				std::placeholders::_1, std::placeholders::_2));
		} catch (std::exception &e) {
			this->log(swarm::LOG_ERROR, "GET request, invalid cast: %s", e.what());
			this->send_reply(swarm::network_reply::bad_request);
		}
	}

	ioremap::elliptics::async_write_result write_data(const swarm::network_request &req, const ioremap::elliptics::data_pointer &data) {
		swarm::network_url url(req.get_url());
		swarm::network_query_list query_list(url.query());

		std::string name = query_list.item_value("name");

		ioremap::elliptics::session sess = this->get_server()->create_session();

		size_t offset = 0;
		if (auto tmp = query_list.try_item("offset"))
			offset = boost::lexical_cast<size_t>(*tmp);

		if (auto tmp = query_list.try_item("prepare")) {
	            size_t size = boost::lexical_cast<size_t>(*tmp);
	            return sess.write_prepare(name, data, offset, size);
	        } else if (auto tmp = query_list.try_item("commit")) {
	            size_t size = boost::lexical_cast<size_t>(*tmp);
	            return sess.write_commit(name, data, offset, size);
	        } else if (query_list.has_item("plain_write") || query_list.has_item("plain-write")) {
	            return sess.write_plain(name, data, offset);
	        } else {
	            return sess.write_data(name, data, offset);
	        }
	}

	static void fill_upload_reply(const ioremap::elliptics::sync_write_result &result, elliptics::JsonValue &result_object) {
		const ioremap::elliptics::write_result_entry &entry = result[0];

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

		rapidjson::Value tobj;
		JsonValue::set_time(tobj, result_object.GetAllocator(),
				entry.file_info()->mtime.tsec,
				entry.file_info()->mtime.tnsec / 1000);
		result_object.AddMember("mtime", tobj, result_object.GetAllocator());

		char addr_str[128];
		dnet_server_convert_dnet_addr_raw(entry.storage_address(), addr_str, sizeof(addr_str));
		
		rapidjson::Value server_addr(addr_str, strlen(addr_str), result_object.GetAllocator());
		result_object.AddMember("server", server_addr, result_object.GetAllocator());
	}

	virtual void on_write_finished(const ioremap::elliptics::sync_write_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::network_reply::service_unavailable);
			return;
		}

		elliptics::JsonValue result_object;
		on_upload::fill_upload_reply(result, result_object);

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
