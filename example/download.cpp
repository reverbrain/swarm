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
	ev::loop_ref &loop;

	void operator() (const ioremap::swarm::url_fetcher::response &reply, const std::string &data, const boost::system::error_code &error) const {
		std::cout << "HTTP code: " << reply.code() << std::endl;
		std::cout << "Error: " << error.message() << std::endl;

		const auto &headers = reply.headers().all();

		for (auto it = headers.begin(); it != headers.end(); ++it) {
			std::cout << "header: \"" << it->first << "\": \"" << it->second << "\"" << std::endl;
		}
		(void) data;
//		std::cout << data << std::endl;

		loop.unloop();
	}
};

class stream : public ioremap::swarm::base_stream
{
public:
	stream(const std::function<void (const ioremap::swarm::http_response &)> &handler) :
		m_response(boost::none),
		m_handler(handler)
	{
	}

	void on_headers(ioremap::swarm::url_fetcher::response &&response)
	{
//		std::cout << "on_headers" << std::endl;
		m_response = std::move(response);
		m_handler(m_response);
	}

	void on_data(const boost::asio::const_buffer &data)
	{
		auto begin = boost::asio::buffer_cast<const char *>(data);
		auto end = begin + boost::asio::buffer_size(data);

//		std::cout << "on_data: \"" << std::string(begin, end) << "\"" << std::endl;
		(void) end;
		(void) data;
	}

	void on_close(const boost::system::error_code &error)
	{
//		std::cout << "on_close: " << error.message() << ", category: " << error.category().name() << std::endl;
		(void) error;
	}

private:
	ioremap::swarm::http_response m_response;
	std::function<void (const ioremap::swarm::http_response &)> m_handler;
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

	const bool use_boost = false;

	boost::asio::io_service service;
	std::unique_ptr<ioremap::swarm::event_loop> loop_impl;
	if (use_boost)
		loop_impl.reset(new ioremap::swarm::boost_event_loop(service));
	else
		loop_impl.reset(new ioremap::swarm::ev_event_loop(loop));

	ioremap::swarm::logger logger("/dev/stdout", ioremap::swarm::SWARM_LOG_NOTICE);

	ioremap::swarm::url_fetcher manager(*loop_impl, logger);

	ioremap::swarm::url_fetcher::request request;
	request.set_url(argv[1]);
	request.set_follow_location(1);
	request.set_timeout(500000);
	request.headers().assign({
		{ "Content-Type", "text/html; always" },
		{ "Additional-Header", "Very long-long\r\n\tsecond line\r\n\tthird line" }
	});

	typedef std::chrono::high_resolution_clock clock;

	auto begin_time = clock::now();

	request_handler_functor request_handler = { loop };

	manager.get(ioremap::swarm::simple_stream::create(request_handler), std::move(request));

	if (use_boost) {
		boost::asio::io_service::work work(service);
		service.run();
	} else {
		loop.loop();
	}

	auto end_time = clock::now();

	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time);
	std::cout << "Finished in: " << ms.count() << " ms" << std::endl;

	return 0;
}
