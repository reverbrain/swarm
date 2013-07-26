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

#include <thevoid/server.hpp>
#include <elliptics/session.hpp>

namespace ioremap {
namespace thevoid {

struct id_comparator
{
	bool operator() (const dnet_raw_id &first, const dnet_raw_id &second) const
	{
		return memcmp(first.id, second.id, sizeof(first.id)) < 0;
	}
};

class elliptics_server : public server<elliptics_server>
{
public:
	elliptics_server();

	bool initialize(const rapidjson::Value &config) /*override*/;

	// set indexes for given ID
	struct on_update : public simple_request_stream<elliptics_server>, public std::enable_shared_from_this<on_update>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_update_finished(const ioremap::elliptics::error_info &error);
	};

	// find (using 'AND' or 'OR' operator) indexes, which contain given ID
	struct on_find : public simple_request_stream<elliptics_server>, public std::enable_shared_from_this<on_find>
	{
		struct read_result_cmp {
			bool operator ()(const ioremap::elliptics::read_result_entry &e1, const ioremap::elliptics::read_result_entry &e2) const {
				return dnet_id_cmp(&e1.command()->id, &e2.command()->id) < 0;
			}

			const dnet_raw_id &operator()(const ioremap::elliptics::find_indexes_result_entry &e) const {
				return e.id;
			}

			bool operator ()(const ioremap::elliptics::read_result_entry &e1, const dnet_raw_id &id) const {
				return dnet_id_cmp_str((const unsigned char *)e1.command()->id.id, id.id) < 0;
			}
		} m_rrcmp;

		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_find_finished(const ioremap::elliptics::sync_find_indexes_result &result, const ioremap::elliptics::error_info &error);
		void on_ready_to_parse_indexes(const ioremap::elliptics::sync_read_result &result, const ioremap::elliptics::error_info &error);

		void send_indexes_reply(ioremap::elliptics::sync_read_result &data, const ioremap::elliptics::sync_find_indexes_result &result);

		std::map<dnet_raw_id, std::string, id_comparator> m_map;
		std::string m_view;
		ioremap::elliptics::sync_find_indexes_result m_result;
	};

	// read data object
	struct on_get : public simple_request_stream<elliptics_server>, public std::enable_shared_from_this<on_get>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_read_finished(const ioremap::elliptics::sync_read_result &result, const ioremap::elliptics::error_info &error);
	};

	// write data object, get file-info json in response
	struct on_upload : public simple_request_stream<elliptics_server>, public std::enable_shared_from_this<on_upload>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_write_finished(const ioremap::elliptics::sync_write_result &result, const ioremap::elliptics::error_info &error);
	};

	struct on_ping : public simple_request_stream<elliptics_server>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
	};

	struct on_echo : public simple_request_stream<elliptics_server>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
	};

protected:
	ioremap::elliptics::session create_session();

private:
	std::unique_ptr<ioremap::elliptics::logger> m_logger;
	std::unique_ptr<ioremap::elliptics::node> m_node;
	std::unique_ptr<ioremap::elliptics::session> m_session;
};

} } // namespace ioremap::thevoid
