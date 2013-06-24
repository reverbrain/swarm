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

class elliptics_server : public server<elliptics_server>
{
public:
	elliptics_server();

	bool initialize(const rapidjson::Value &config) /*override*/;

	struct on_update : public simple_request_stream<elliptics_server>, public std::enable_shared_from_this<on_update>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_update_finished(const ioremap::elliptics::error_info &error);
		virtual void on_close(const boost::system::error_code &err) /*override*/;
	};

	struct on_find : public simple_request_stream<elliptics_server>, public std::enable_shared_from_this<on_find>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_find_finished(const ioremap::elliptics::sync_find_indexes_result &result, const ioremap::elliptics::error_info &error);
		virtual void on_close(const boost::system::error_code &err) /*override*/;
	};

	struct on_get : public simple_request_stream<elliptics_server>, public std::enable_shared_from_this<on_get>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_read_finished(const ioremap::elliptics::sync_read_result &result, const ioremap::elliptics::error_info &error);
		virtual void on_send_finished(const ioremap::elliptics::data_pointer &);
		virtual void on_close(const boost::system::error_code &err) /*override*/;
	};

	struct on_upload : public simple_request_stream<elliptics_server>, public std::enable_shared_from_this<on_upload>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_write_finished(const ioremap::elliptics::sync_write_result &result, const ioremap::elliptics::error_info &error);
		virtual void on_close(const boost::system::error_code &err) /*override*/;
	};

	struct on_ping : public simple_request_stream<elliptics_server>
	{
		virtual void on_request(const swarm::network_request &req, const boost::asio::const_buffer &buffer) /*override*/;
		virtual void on_close(const boost::system::error_code &err) /*override*/;
	};

protected:
	ioremap::elliptics::session create_session();

private:
	std::unique_ptr<ioremap::elliptics::logger> m_logger;
	std::unique_ptr<ioremap::elliptics::node> m_node;
	std::unique_ptr<ioremap::elliptics::session> m_session;
};

} } // namespace ioremap::thevoid
