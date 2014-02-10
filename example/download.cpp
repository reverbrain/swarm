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

#include <swarm/urlfetcher/url_fetcher.hpp>
#include <swarm/urlfetcher/boost_event_loop.hpp>
#include <swarm/urlfetcher/ev_event_loop.hpp>
#include <swarm/urlfetcher/stream.hpp>
#include <swarm/logger.hpp>
#include <swarm/c++config.hpp>
#include <list>
#include <iostream>
#include <chrono>
#include <thread>

#include <blackhole/log.hpp>
#include <blackhole/repository.hpp>

#ifdef SWARM_CSTDATOMIC
#  include <cstdatomic>
#else
#  include <atomic>
#endif

#define USE_BOOST

struct sig_handler
{
	ev::loop_ref &loop;

	void operator() (ev::sig &, int) {
		loop.unloop();
	}
};

#ifdef __linux__
# include <sys/prctl.h>

void set_thread_name(const char *name)
{
	prctl(PR_SET_NAME, name);
}
#else
void set_thread_name(const char *)
{
}
#endif


struct request_handler_functor
{
#ifdef USE_BOOST
	boost::asio::io_service& service;
	blackhole::verbose_logger_t<ioremap::swarm::log_level>& log;
#else
	ev::loop_ref &loop;
#endif

	void operator() (const ioremap::swarm::url_fetcher::response &reply, const std::string &data, const boost::system::error_code &error) const {
		BH_LOG(log, ioremap::swarm::SWARM_LOG_INFO, "resuest finished")(
			blackhole::attribute::make("request url", reply.request().url().to_string()),
			blackhole::attribute::make("reply url", reply.url().to_string()),
			blackhole::attribute::make("status", reply.code()),
			blackhole::attribute::make("error", error.message())
		);

		const auto &headers = reply.headers().all();

		for (auto it = headers.begin(); it != headers.end(); ++it) {
			BH_LOG(log, ioremap::swarm::SWARM_LOG_INFO, "header: %s : %s ", it->first, it->second);
		}
		(void) data;
#ifdef USE_BOOST
		service.stop();
#else
		loop.unloop();
#endif
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
	std::unique_ptr<ioremap::swarm::event_loop> loop_impl;
#ifdef USE_BOOST
	loop_impl.reset(new ioremap::swarm::boost_event_loop(service));
#else
	loop_impl.reset(new ioremap::swarm::ev_event_loop(loop));
#endif

	//! Blackhole initialization
	blackhole::formatter_config_t formatter("string");
	formatter["pattern"] = "[%(timestamp)s] [%(severity)s]: %(message)s [%(...L)s]";

	blackhole::sink_config_t sink("files");
	sink["path"] = "/dev/stdout";
	sink["autoflush"] = true;

	blackhole::frontend_config_t frontend = { formatter, sink };
	blackhole::log_config_t config{ "root", { frontend } };

	ioremap::swarm::logger logger(config, ioremap::swarm::SWARM_LOG_DEBUG);
	blackhole::verbose_logger_t<ioremap::swarm::log_level> log =
			blackhole::repository_t<ioremap::swarm::log_level>::instance().create(config.name);

	ioremap::swarm::url_fetcher manager(*loop_impl, logger);

	ioremap::swarm::url_fetcher::request request;
	request.set_url(argv[1]);
	request.set_follow_location(1);
	request.set_timeout(1000);
	request.headers().assign({
		{ "Content-Type", "text/html; always" },
		{ "Additional-Header", "Very long-long\r\n\tsecond line\r\n\tthird line" }
	});

	typedef std::chrono::high_resolution_clock clock;

	auto begin_time = clock::now();

#ifdef USE_BOOST
	request_handler_functor request_handler = { service, log };
#else
	request_handler_functor request_handler = { loop };
#endif

	manager.get(ioremap::swarm::simple_stream::create(request_handler), std::move(request));

#ifdef USE_BOOST
	boost::asio::io_service::work work(service);
	service.run();
#else
	loop.loop();
#endif

	auto end_time = clock::now();

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time);
	BH_LOG(log, ioremap::swarm::SWARM_LOG_INFO, "finished in: %d ms", ms.count());

	return 0;
}
