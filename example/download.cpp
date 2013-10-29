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
#include <list>
#include <iostream>
#include <chrono>
#include <thread>

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

	void operator() (const ioremap::swarm::url_fetcher::response &reply) const {
		std::cout << "HTTP code: " << reply.code() << std::endl;

		const auto &headers = reply.headers().all();

		for (auto it = headers.begin(); it != headers.end(); ++it) {
			std::cout << "header: \"" << it->first << "\": \"" << it->second << "\"" << std::endl;
		}

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

	ioremap::swarm::logger logger("/dev/stdout", ioremap::swarm::LOG_ERROR);

	ioremap::swarm::url_fetcher manager(*loop_impl, logger);

	std::vector<ioremap::swarm::headers_entry> headers = {
		{ "Content-Type", "text/html; always" },
		{ "Additional-Header", "Very long-long\r\n\tsecond line\r\n\tthird line" }
	};

	ioremap::swarm::url_fetcher::request request;
	request.set_url(argv[1]);
	request.set_follow_location(1);
	request.set_timeout(500000);
	request.headers().set(headers);

	typedef std::chrono::high_resolution_clock clock;

	auto begin_time = clock::now();

//	request_handler_functor request_handler = { loop };

//	manager.get(request_handler, request);

	std::thread thread([&manager] () {
		set_thread_name("swarm_requester");
		ioremap::swarm::url_fetcher::request request;
		request.set_url("http://localhost:8080/echo");
//		request.headers().set_keep_alive();

		for (size_t j = 1; j < 1; ++j) {
			size_t total_count = 5000000;
			auto total_begin = clock::now();

			size_t total_mu = 0;
			size_t min_mu = std::numeric_limits<size_t>::max();
			size_t max_mu = std::numeric_limits<size_t>::min();

			auto timer_begin = clock::now();
			std::atomic_int counter(0);

			for (size_t i = 0; i < 1; ++i) {
				auto begin = clock::now();
				auto handler = std::make_shared<stream>([&counter, &total_mu, &min_mu, &max_mu, &timer_begin, begin] (const ioremap::swarm::http_response &reply) {
					auto end = clock::now();
					auto mu = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
					total_mu += mu.count();
					min_mu = std::min(min_mu, total_mu);
					max_mu = std::max(max_mu, total_mu);
//					std::cout << "reply: " << reply.code() << ", " << mu.count() / 1000. << " ms" << std::endl;
					(void) reply;

					++counter;


					auto timer_end = clock::now();
					auto timer_s = std::chrono::duration_cast<std::chrono::seconds>(timer_end - timer_begin).count();
					if (timer_s >= 1) {
						timer_begin = timer_end;
						std::cout << counter << " rps" << std::endl;
						counter = 0;
					}
				});
				ioremap::swarm::url_fetcher::request tmp = request;
				manager.get(std::move(handler), std::move(tmp));
				usleep(40);
			}
			auto total_end = clock::now();

			sleep(1);

			std::cout << "count: " << total_count
				  << ", total_send: " << std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_begin).count() / 1000. << " ms"
				  << ", avg: " << total_mu / (total_count * 1000.) << " ms"
				  << ", max: " << max_mu / 1000. << " ms"
				  << ", min: " << min_mu / 1000. << " ms"
				  << std::endl;
		}

		std::cout << "Sent all" << std::endl;
	});

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
