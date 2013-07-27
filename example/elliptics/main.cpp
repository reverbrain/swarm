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

using namespace ioremap::thevoid;

class example_server : public server<example_server>, public elliptics_server
{
public:

	virtual bool initialize(const rapidjson::Value &config) {
		elliptics_server::initialize(config);

		on<elliptics::index::on_update<example_server>>("/update");
		on<elliptics::index::on_find<example_server>>("/find");
		on<elliptics::io::on_get<example_server>>("/get");
		on<elliptics::io::on_upload<example_server>>("/upload");
		on<elliptics::common::on_ping<example_server>>("/ping");
		on<elliptics::common::on_echo<example_server>>("/echo");
	
		return true;
	}
};

int main(int argc, char **argv)
{
	return run_server<example_server>(argc, argv);
}
