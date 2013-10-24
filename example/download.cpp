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

#include <swarm/manager/access_manager.hpp>
#include <swarm/manager/boost_event_loop.hpp>
#include <swarm/manager/ev_event_loop.hpp>
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

	void operator() (const ioremap::swarm::http_response &reply) const {
		std::cout << "HTTP code: " << reply.code() << std::endl;
		std::cout << "Network error: " << reply.error() << std::endl;

		const auto &headers = reply.headers().all();

		for (auto it = headers.begin(); it != headers.end(); ++it) {
			std::cout << "header: \"" << it->first << "\": \"" << it->second << "\"" << std::endl;
		}
		std::cout << "data: " << reply.data() << std::endl;

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

	boost::asio::io_service service;
	ioremap::swarm::boost_event_loop boost_loop(service);

	ioremap::swarm::logger logger("/dev/stdout", ioremap::swarm::LOG_DEBUG);

	ioremap::swarm::access_manager manager(boost_loop, logger);

	std::vector<ioremap::swarm::headers_entry> headers = {
		{ "Content-Type", "text/html; always" },
		{ "Additional-Header", "Very long-long\r\n\tsecond line\r\n\tthird line" }
	};

	ioremap::swarm::http_request request;
	request.set_url(argv[1]);
	request.set_follow_location(1);
	request.set_timeout(500000);
	request.headers().set(headers);

	typedef std::chrono::high_resolution_clock clock;

	auto begin_time = clock::now();

	request_handler_functor request_handler = { loop };

	manager.get(request_handler, request);

	boost::asio::io_service::work work(service);
	service.run();
//	loop.loop();

	auto end_time = clock::now();

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time);
	std::cout << "Finished in: " << ms.count() << " ms" << std::endl;

	return 0;
}
