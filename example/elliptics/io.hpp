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

#include <swarm/url.hpp>
#include <swarm/url_query.hpp>

#include <thevoid/server.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "jsonvalue.hpp"

namespace ioremap { namespace thevoid { namespace elliptics { namespace io {

static boost::optional<ioremap::elliptics::key> get_key(const swarm::http_request &request)
{
	const auto &query = request.url().query();

	if (auto name = query.item_value("name")) {
		return ioremap::elliptics::key(*name);
	} else if (auto sid = query.item_value("id")) {
		struct dnet_id id;
		memset(&id, 0, sizeof(struct dnet_id));

		dnet_parse_numeric_id(sid->c_str(), id.id);

		return ioremap::elliptics::key(id);
	} else {
		return boost::none;
	}
}

static ioremap::elliptics::data_pointer create_data(const boost::asio::const_buffer &buffer)
{
	return ioremap::elliptics::data_pointer::from_raw(
		const_cast<char *>(boost::asio::buffer_cast<const char*>(buffer)),
		boost::asio::buffer_size(buffer)
	);
}

// read data object
template <typename T>
struct on_get : public simple_request_stream<T>, public std::enable_shared_from_this<on_get<T>>
{
	virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) {
		using namespace std::placeholders;

		(void) buffer;

		const auto &query = req.url().query();

		ioremap::elliptics::session sess = this->server()->create_session();

		size_t offset = 0;
		size_t size = 0;

		try {
			if (auto tmp = query.item_value("offset"))
				offset = boost::lexical_cast<size_t>(*tmp);

			if (auto tmp = query.item_value("size"))
				size = boost::lexical_cast<size_t>(*tmp);
		} catch (std::exception &e) {
			this->log(swarm::LOG_ERROR, "GET request, invalid cast: %s", e.what());
			this->send_reply(swarm::http_response::bad_request);
			return;
		}

		if (auto name = query.item_value("name")) {
			this->log(swarm::LOG_DEBUG, "GET request, name: \"%s\"", name->c_str());

			sess.read_data(*name, offset, size).connect(
					std::bind(&on_get::on_read_finished, this->shared_from_this(), _1, _2));
		} else if (auto sid = query.item_value("id")) {
			struct dnet_id id;
			memset(&id, 0, sizeof(struct dnet_id));

			dnet_parse_numeric_id(sid->c_str(), id.id);
			sess.read_data(id, 0, 0).connect(
					std::bind(&on_get::on_read_finished, this->shared_from_this(), _1, _2));
		} else {
			this->send_reply(swarm::http_response::bad_request);
		}
	}

	virtual void on_read_finished(const ioremap::elliptics::sync_read_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error.code() == -ENOENT) {
			this->send_reply(swarm::http_response::not_found);
			return;
		} else if (error) {
			this->send_reply(swarm::http_response::service_unavailable);
			return;
		}

		const ioremap::elliptics::read_result_entry &entry = result[0];

		ioremap::elliptics::data_pointer file = entry.file();

		const dnet_time &ts = entry.io_attribute()->timestamp;
		const swarm::http_request &request = this->request();

		if (auto tmp = request.headers().if_modified_since()) {
			if ((time_t)ts.tsec <= *tmp) {
				this->send_reply(swarm::http_response::not_modified);
				return;
			}
		}

		if (auto tmp = request.headers().get("Range")) {
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

		swarm::http_response reply;
		reply.set_code(swarm::http_response::ok);
		reply.headers().set_content_length(file.size());
		reply.headers().set_content_type("text/plain");
		reply.headers().set_last_modified(ts.tsec);

		this->send_reply(std::move(reply), std::move(file));
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
			this->send_reply(swarm::http_response::requested_range_not_satisfiable);
			return;
		}

		auto data_part = data.slice(begin, end + 1 - begin);

		swarm::http_response reply;
		reply.set_code(swarm::http_response::partial_content);
		reply.headers().set_content_type("text/plain");
		reply.headers().set_last_modified(ts.tsec);
		reply.headers().add("Accept-Ranges", "bytes");
		reply.headers().add("Content-Range", create_content_range(begin, end, data.size()));
		reply.headers().set_content_length(data_part.size());

		this->send_reply(std::move(reply), std::move(data_part));
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
			this->send_reply(swarm::http_response::requested_range_not_satisfiable);
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

		swarm::http_response reply;
		reply.set_code(swarm::http_response::partial_content);
		reply.headers().set_content_type(std::string("multipart/byteranges; boundary=") + boundary);
		reply.headers().set_last_modified(ts.tsec);
		reply.headers().add("Accept-Ranges", "bytes");
		reply.headers().set_content_length(result.size());

		this->send_reply(std::move(reply), std::move(result));
	}
};

// write data object, get file-info json in response
template <typename T>
struct on_upload : public simple_request_stream<T>, public std::enable_shared_from_this<on_upload<T>>
{
	virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) {
		auto data = ioremap::elliptics::data_pointer::from_raw(
			const_cast<char *>(boost::asio::buffer_cast<const char*>(buffer)),
			boost::asio::buffer_size(buffer));

		try {
			write_data(req, data).connect(
				std::bind(&on_upload::on_write_finished, this->shared_from_this(),
				std::placeholders::_1, std::placeholders::_2));
		} catch (std::exception &e) {
			this->log(swarm::LOG_ERROR, "GET request, invalid cast: %s", e.what());
			this->send_reply(swarm::http_response::bad_request);
		}
	}

	ioremap::elliptics::async_write_result write_data(const swarm::http_request &req, const ioremap::elliptics::data_pointer &data) {
		const auto &query = req.url().query();

		auto possible_name = query.item_value("name");
		if (!possible_name) {
			throw std::runtime_error("invalid name");
		}

		auto name = *possible_name;

		ioremap::elliptics::session sess = this->server()->create_session();

		size_t offset = 0;
		if (auto tmp = query.item_value("offset"))
			offset = boost::lexical_cast<size_t>(*tmp);

		if (auto tmp = query.item_value("prepare")) {
				size_t size = boost::lexical_cast<size_t>(*tmp);
				return sess.write_prepare(name, data, offset, size);
			} else if (auto tmp = query.item_value("commit")) {
				size_t size = boost::lexical_cast<size_t>(*tmp);
				return sess.write_commit(name, data, offset, size);
			} else if (query.has_item("plain-write")) {
				return sess.write_plain(name, data, offset);
			} else {
				return sess.write_data(name, data, offset);
			}
	}

	template <typename Allocator>
	static void fill_upload_reply(const ioremap::elliptics::write_result_entry &entry, rapidjson::Value &result_object, Allocator &allocator) {
		char id_str[2 * DNET_ID_SIZE + 1];
		dnet_dump_id_len_raw(entry.command()->id.id, DNET_ID_SIZE, id_str);
		rapidjson::Value id_str_value(id_str, 2 * DNET_ID_SIZE, allocator);
		result_object.AddMember("id", id_str_value, allocator);

		char csum_str[2 * DNET_ID_SIZE + 1];
		dnet_dump_id_len_raw(entry.file_info()->checksum, DNET_ID_SIZE, csum_str);
		rapidjson::Value csum_str_value(csum_str, 2 * DNET_ID_SIZE, allocator);
		result_object.AddMember("csum", csum_str_value, allocator);

		if (entry.file_path())
			result_object.AddMember("filename", entry.file_path(), allocator);

		result_object.AddMember("size", entry.file_info()->size, allocator);
		result_object.AddMember("offset-within-data-file", entry.file_info()->offset,
				allocator);

		rapidjson::Value tobj;
		JsonValue::set_time(tobj, allocator,
				entry.file_info()->mtime.tsec,
				entry.file_info()->mtime.tnsec / 1000);
		result_object.AddMember("mtime", tobj, allocator);

		char addr_str[128];
		dnet_server_convert_dnet_addr_raw(entry.storage_address(), addr_str, sizeof(addr_str));
		
		rapidjson::Value server_addr(addr_str, strlen(addr_str), allocator);
		result_object.AddMember("server", server_addr, allocator);
	}

	template <typename Allocator>
	static void fill_upload_reply(const ioremap::elliptics::sync_write_result &result, rapidjson::Value &result_object, Allocator &allocator) {
		rapidjson::Value infos;
		infos.SetArray();

		for (auto it = result.begin(); it != result.end(); ++it) {
			rapidjson::Value download_info;
			download_info.SetObject();

			fill_upload_reply(*it, download_info, allocator);

			infos.PushBack(download_info, allocator);
		}

		result_object.AddMember("info", infos, allocator);
	}

	virtual void on_write_finished(const ioremap::elliptics::sync_write_result &result,
			const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::http_response::service_unavailable);
			return;
		}

		elliptics::JsonValue result_object;
		on_upload::fill_upload_reply(result, result_object, result_object.GetAllocator());

		auto data = result_object.ToString();

		swarm::http_response reply;
		reply.set_code(swarm::http_response::ok);
		reply.headers().set_content_type("text/json");
		reply.headers().set_content_length(data.size());

		this->send_reply(std::move(reply), std::move(data));
	}
};

// write data object, get file-info json in response
template <typename T>
struct on_buffered_upload : public buffered_request_stream<T>, public std::enable_shared_from_this<on_buffered_upload<T>>
{
public:
	virtual void on_request(const swarm::http_request &request)
	{
		const auto &query = request.url().query();
		this->set_chunk_size(10 * 1024 * 1024);

		auto key = get_key(request);

		if (!key) {
			this->send_reply(swarm::http_response::bad_request);
			return;
		}

		m_offset = query.item_value("offset", 0llu);
		if (auto size = request.headers().content_length())
			m_size = *size;
		else
			m_size = 0;
		m_key = *key;
	}

	virtual void on_chunk(const boost::asio::const_buffer &buffer, unsigned int flags)
	{
		ioremap::elliptics::session sess = this->server()->create_session();
		const auto data = create_data(buffer);

		this->log(swarm::LOG_INFO, "on_chunk: size: %zu, m_offset: %llu, flags: %u", data.size(), (unsigned long long)m_offset, flags);

		if (flags & buffered_request_stream<T>::first_chunk) {
			m_groups = sess.get_groups();
		} else {
			sess.set_groups(m_groups);
		}

		ioremap::elliptics::async_write_result result = write(sess, data, flags);
		m_offset += data.size();

		using namespace std::placeholders;
		if (flags & buffered_request_stream<T>::last_chunk) {
			result.connect(std::bind(&on_buffered_upload::on_write_finished, this->shared_from_this(), _1, _2));
		} else {
			result.connect(std::bind(&on_buffered_upload::on_write_partial, this->shared_from_this(), _1, _2));
		}
	}

	ioremap::elliptics::async_write_result write(ioremap::elliptics::session &sess,
		const ioremap::elliptics::data_pointer &data,
		unsigned int flags)
	{
		typedef unsigned long long ull;

		if (flags == buffered_request_stream<T>::single_chunk) {
			return sess.write_data(m_key, data, m_offset);
		} else if (m_size > 0) {
			if (flags & buffered_request_stream<T>::first_chunk) {
				this->log(swarm::LOG_INFO, "prepare, offset: %llu, size: %llu", ull(m_offset), ull(m_size));
				return sess.write_prepare(m_key, data, m_offset, m_size);
			} else if (flags & buffered_request_stream<T>::last_chunk) {
				this->log(swarm::LOG_INFO, "commit, offset: %llu, size: %llu", ull(m_offset), ull(m_offset + data.size()));
				return sess.write_commit(m_key, data, m_offset, m_offset + data.size());
			} else {
				this->log(swarm::LOG_INFO, "plain, offset: %llu", ull(m_offset));
				return sess.write_plain(m_key, data, m_offset);
			}
		} else {
			this->log(swarm::LOG_INFO, "write_data, offset: %llu", ull(m_offset));
			return sess.write_data(m_key, data, m_offset);
		}
	}

	virtual void on_error(const boost::system::error_code &err)
	{
		this->log(swarm::LOG_ERROR, "on_error, error: %s", err.message().c_str());
	}

	virtual void on_write_partial(const ioremap::elliptics::sync_write_result &result, const ioremap::elliptics::error_info &error)
	{
		this->log(swarm::LOG_ERROR, "on_write_partial, error: %s", error.message().c_str());

		if (error) {
			on_write_finished(result, error);
			return;
		}

		std::vector<int> groups;

		for (auto it = result.begin(); it != result.end(); ++it) {
			ioremap::elliptics::write_result_entry entry = *it;

			if (!entry.error())
				groups.push_back(entry.command()->id.group_id);
		}

		using std::swap;
		swap(m_groups, groups);

		this->try_next_chunk();
	}

	virtual void on_write_finished(const ioremap::elliptics::sync_write_result &result, const ioremap::elliptics::error_info &error)
	{
		this->log(swarm::LOG_ERROR, "on_write_finished, error: %s", error.message().c_str());

		if (error) {
			this->send_reply(swarm::http_response::internal_server_error);
			return;
		}

		this->send_reply(swarm::http_response::ok);
	}

private:
	std::vector<int> m_groups;
	ioremap::elliptics::key m_key;
	uint64_t m_offset;
	uint64_t m_size;
};

// perform lookup, get file-info json in response
template <typename T>
struct on_download_info : public simple_request_stream<T>, public std::enable_shared_from_this<on_download_info<T>>
{
	virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &) {
		const auto &query = req.url().query();

		ioremap::elliptics::session sess = this->server()->create_session();

		auto name = query.item_value("name");
		if (!name) {
			this->send_reply(swarm::http_response::bad_request);
			return;
		}

		using namespace std::placeholders;
		sess.lookup(*name).connect(std::bind(&on_download_info::on_lookup_finished, this->shared_from_this(), _1, _2));
	}

	std::string generate_signature(const ioremap::elliptics::lookup_result_entry &entry, const std::string &time, std::string *url_ptr) {
		const auto name = this->request().url().query().item_value("name");
		auto key = this->server()->find_signature(*name);

		if (!key && !url_ptr) {
			return std::string();
		}

		const dnet_file_info *info = entry.file_info();

		std::string url = this->server()->generate_url_base(entry.address());
		swarm::url_query query;
		query.add_item("file-path", entry.file_path());
		if (key) {
			query.add_item("key", *key);
		}
		query.add_item("offset", boost::lexical_cast<std::string>(info->offset));
		query.add_item("size", boost::lexical_cast<std::string>(info->size));
		query.add_item("time", time);

		auto sign_input = url + "?" + query.to_string();

		if (!key) {
			*url_ptr = std::move(sign_input);
			return std::string();
		}

		dnet_raw_id signature_id;
		dnet_transform_node(this->server()->get_node().get_native(),
					sign_input.c_str(), sign_input.size(),
					signature_id.id, sizeof(signature_id.id));

		char signature_str[2 * DNET_ID_SIZE + 1];
		dnet_dump_id_len_raw(signature_id.id, DNET_ID_SIZE, signature_str);

		const std::string signature(signature_str, 2 * DNET_ID_SIZE);

		query.add_item("signature", signature);

		if (url_ptr) {
			// index of "key"
			query.remove_item(1);

			url += "?";
			url += query.to_string();
			*url_ptr = std::move(url);
		}

		return std::move(signature);
	}

	virtual void on_lookup_finished(const ioremap::elliptics::sync_lookup_result &result,
		const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::http_response::service_unavailable);
			return;
		}

		elliptics::JsonValue result_object;
		on_upload<T>::fill_upload_reply(result[0], result_object, result_object.GetAllocator());

		dnet_time time;
		dnet_current_time(&time);
		const std::string time_str = boost::lexical_cast<std::string>(time.tsec);

		std::string signature = generate_signature(result[0], time_str, NULL);

		if (!signature.empty()) {
			rapidjson::Value signature_value(signature.c_str(), signature.size(), result_object.GetAllocator());
			result_object.AddMember("signature", signature_value, result_object.GetAllocator());
		}

		result_object.AddMember("time", time_str.c_str(), result_object.GetAllocator());

		auto data = result_object.ToString();

		swarm::http_response reply;
		reply.set_code(swarm::http_response::ok);
		reply.headers().set_content_type("text/json");
		reply.headers().set_content_length(data.size());

		this->send_reply(std::move(reply), std::move(data));
	}
};

// perform lookup, redirect in response
template <typename T>
struct on_redirectable_get : public on_download_info<T>
{

	virtual void on_lookup_finished(const ioremap::elliptics::sync_lookup_result &result,
		const ioremap::elliptics::error_info &error) {
		if (error) {
			this->send_reply(swarm::http_response::service_unavailable);
			return;
		}

		dnet_time time;
		dnet_current_time(&time);
		const std::string time_str = boost::lexical_cast<std::string>(time.tsec);

		std::string url;

		this->generate_signature(result[0], time_str, &url);

		swarm::http_response reply;
		reply.set_code(swarm::http_response::moved_temporarily);
		reply.headers().set("Location", url);
		reply.headers().set_content_length(0);
	}
};

}}}}	

#endif /*__IOREMAP_THEVOID_ELLIPTICS_IO_HPP */
