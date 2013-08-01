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

#include <swarm/networkmanager.h>
#include <list>
#include <iostream>
#include <chrono>

struct sig_handler
{
	ev::loop_ref &loop;

	void operator() (ev::sig &, int) {
		loop.unloop();
	}
};

struct request_handler_functor
{
	ev::loop_ref &loop;

	void operator() (const ioremap::swarm::network_reply &reply) const {
		std::cout << "HTTP code: " << reply.get_code() << std::endl;
			std::cout << "Network error: " << reply.get_error() << std::endl;

		const auto &headers = reply.get_headers();

		for (auto it = headers.begin(); it != headers.end(); ++it) {
			std::cout << "header: \"" << it->first << "\": \"" << it->second << "\"" << std::endl;
		}
		std::cout << "data: " << reply.get_data() << std::endl;

		loop.unloop();
	}
};

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " url" << std::endl;
		return 1;
	}

	ev::default_loop loop;

	sig_handler shandler = { loop };
	std::list<ev::sig> sigs;
	int signal_ids[] = { SIGINT, SIGTERM };

	for (size_t i = 0; i < sizeof(signal_ids) / sizeof(signal_ids[0]); ++i) {
		sigs.emplace_back(loop);
		ev::sig &sig_watcher = sigs.back();
	sig_watcher.set(signal_ids[i]);
		sig_watcher.set(&shandler);
		sig_watcher.start();
	}

	ioremap::swarm::network_manager manager(loop);

	std::vector<ioremap::swarm::headers_entry> headers = {
		{ "Content-Type", "text/html; always" },
		{ "Additional-Header", "Very long-long\r\n\tsecond line\r\n\tthird line" }
	};

	ioremap::swarm::network_request request;
	request.set_url(argv[1]);
	request.set_follow_location(1);
	request.set_timeout(5000);
	request.set_headers(headers);

	typedef std::chrono::high_resolution_clock clock;

	auto begin_time = clock::now();

	request_handler_functor request_handler = { loop };

	manager.get(request_handler, request);

	loop.loop();

	auto end_time = clock::now();

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time);
	std::cout << "Finished in: " << ms.count() << " ms" << std::endl;

	return 0;
}
