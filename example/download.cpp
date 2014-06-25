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

#ifdef SWARM_CSTDATOMIC
#  include <cstdatomic>
#else
#  include <atomic>
#endif

#include <boost/version.hpp>

#include <blackhole/blackhole.hpp>

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

#if BOOST_VERSION / 100 % 1000 >= 47
#define USE_BOOST_SIGNALS
#endif

struct request_handler_functor
{
	ev::loop_ref &loop;
	blackhole::verbose_logger_t<ioremap::swarm::log_level> &log;

	void operator() (const ioremap::swarm::url_fetcher::response &reply, const std::string &data, const boost::system::error_code &error) const {
		const auto &headers = reply.headers().all();
		BH_LOG(log, ioremap::swarm::SWARM_LOG_INFO, "request finished")(blackhole::attribute::list({
			{ "request url", reply.request().url().to_string() },
			{ "reply url", reply.url().to_string() },
			{ "status", reply.code() },
			{ "error", error.message() }
		}));

		for (auto it = headers.begin(); it != headers.end(); ++it) {
			BH_LOG(log, ioremap::swarm::SWARM_LOG_INFO, "header: %s : %s ", it->first, it->second);
		}
		(void) data;

		loop.unloop();
	}
};

#ifdef USE_BOOST_SIGNALS
static void test_signals(boost::asio::io_service *service,
		const boost::system::error_code& error, // Result of operation.
		int signal_number) // Indicates which signal occurred.
{
	printf("called signals function: signal: %d, error: %s\n", signal_number, error.message().c_str());
	if (!error)
		// this is thread-safe operation
		service->stop();
}
#endif

int main(int argc, char **argv)
{
	printf("boost version: %d\n", BOOST_VERSION / 100 % 1000);
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

	const bool use_boost = true;

	boost::asio::io_service service;
#ifdef USE_BOOST_SIGNALS
	boost::asio::signal_set signals(service, SIGINT, SIGTERM);
	signals.async_wait(std::bind(&test_signals, &service, std::placeholders::_1, std::placeholders::_2));
#endif
	std::unique_ptr<ioremap::swarm::event_loop> loop_impl;
	if (use_boost) {
		loop_impl.reset(new ioremap::swarm::boost_event_loop(service));
	} else {
		loop_impl.reset(new ioremap::swarm::ev_event_loop(loop));
	}

	// Blackhole initialization
	blackhole::formatter_config_t formatter("string");
	formatter["pattern"] = "[%(timestamp)s] [%(severity)s]: %(message)s [%(...L)s]";

	blackhole::sink_config_t sink("stream");
	sink["output"] = "stdout";

	blackhole::frontend_config_t frontend = { formatter, sink };
	blackhole::log_config_t config{ "default", { frontend } };

	auto& repository = blackhole::repository_t::instance();
	ioremap::swarm::logger logger(config, ioremap::swarm::SWARM_LOG_DEBUG);
	ioremap::swarm::url_fetcher manager(*loop_impl, logger);
	blackhole::verbose_logger_t<ioremap::swarm::log_level> log =
		repository.create<ioremap::swarm::log_level>(config.name);

	ioremap::swarm::url_fetcher::request request;
	request.set_url(argv[1]);
	request.set_follow_location(1);
	request.set_timeout(100); // in milliseconds
	request.headers().assign({
		{ "Content-Type", "text/html; always" },
		{ "Additional-Header", "Very long-long\r\n\tsecond line\r\n\tthird line" }
	});

	typedef std::chrono::high_resolution_clock clock;

	auto begin_time = clock::now();

	request_handler_functor request_handler = { loop, log };

	manager.get(ioremap::swarm::simple_stream::create(request_handler), std::move(request));

	if (use_boost) {
		boost::asio::io_service::work work(service);
		service.run();
	} else {
		loop.loop();
	}

	auto end_time = clock::now();

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time);
	BH_LOG(log, ioremap::swarm::SWARM_LOG_INFO, "finished in: %d ms", ms.count());

	return 0;
}
