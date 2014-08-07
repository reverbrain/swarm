/*
 * Copyright 2013+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <swarm/urlfetcher/url_fetcher.hpp>
#include <swarm/urlfetcher/boost_event_loop.hpp>
#include <swarm/urlfetcher/ev_event_loop.hpp>
#include <swarm/urlfetcher/stream.hpp>
#include <list>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <iostream>
#include <blackhole/utils/atomic.hpp>

#include <boost/program_options.hpp>
#include <boost/thread.hpp>

#include "timer.hpp"

using namespace ioremap;

struct request_handler_shared {
};

struct request_handler_functor {
	request_handler_functor() : finished(false), counter(0), total(0)
	{
	}

	std::mutex mutex;
	std::condition_variable condition;
	bool finished;
	std::atomic_long counter;
	long total;

	void operator() (const swarm::url_fetcher::response &reply, const std::string &data, const boost::system::error_code &error) {
#if 0
		std::cout << "HTTP code: " << reply.code() << std::endl;
		std::cout << "Error: " << error.message() << std::endl;

		const auto &headers = reply.headers().all();

		for (auto it = headers.begin(); it != headers.end(); ++it) {
			std::cout << "header: \"" << it->first << "\": \"" << it->second << "\"" << std::endl;
		}

		std::cout << "data: " << data << std::endl;
#else
		(void) reply;
		(void) data;
		(void) error;
#endif
		if (++counter == total) {
			std::unique_lock<std::mutex> locker(mutex);
			finished = true;
			condition.notify_all();
		}
	}
};

struct io_service_runner
{
	boost::asio::io_service *service;

	void operator()() const
	{
		service->run();
	}
};

int main(int argc, char *argv[])
{
        namespace bpo = boost::program_options;

        bpo::options_description generic("Cocaine-service testing options");

        std::string url;

	long request_num, chunk_num, connections_limit;

        generic.add_options()
                ("help", "This help message")
                ("url", bpo::value<std::string>(&url)->default_value("http://localhost:8080/get"), "Test URL for GET request")
                ("requests", bpo::value<long>(&request_num)->default_value(100000), "Number of test calls")
                ("chunk", bpo::value<long>(&chunk_num)->default_value(1000), "Send this many requests and then synchronously wait for all of them to complete")
		("connections", bpo::value<long>(&connections_limit)->default_value(100), "Number of connections limit")
                ;

        bpo::options_description cmdline_options;
        cmdline_options.add(generic);

        try {
                bpo::variables_map vm;
                bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).run(), vm);
                bpo::notify(vm);

                if (vm.count("help")) {
                        std::cerr << cmdline_options << std::endl;
                        return -1;
                }
        } catch (...) {
                std::cerr << cmdline_options << std::endl;
                return -1;
        }

	auto logger_base = ioremap::swarm::utils::logger::create("/dev/stdout", SWARM_LOG_DEBUG);
	ioremap::swarm::logger logger(logger_base, blackhole::log::attributes_t());

	boost::asio::io_service service;
	std::unique_ptr<boost::asio::io_service::work> work;
	work.reset(new boost::asio::io_service::work(service));
	swarm::boost_event_loop loop(service, logger);

	swarm::url_fetcher manager(loop, logger);
	manager.set_total_limit(connections_limit);
//	manager.set_host_limit(connections_limit);

	io_service_runner runner = { &service };
	boost::thread thread(runner);

	ioremap::warp::timer tm, total, preparation;

	for (long i = 0; i < request_num;) {
		preparation.restart();

		request_handler_functor handler;
		handler.total = std::min(chunk_num, request_num - i);

		for (long j = 0; j < handler.total; ++i, ++j) {
			swarm::url_fetcher::request request;
			request.set_url(url);
			request.set_timeout(500000);

			manager.get(swarm::simple_stream::create(std::ref(handler)), std::move(request));
		}

		auto preparation_usecs = preparation.elapsed();

		std::unique_lock<std::mutex> locker(handler.mutex);
		while (!handler.finished) {
			handler.condition.wait(locker);
		}

		auto tm_result = tm.restart();
		std::cout << "num: " << handler.total << ", performance: " << handler.total * 1000000 / tm_result << ", time: " << tm_result << " usecs"
			  << ", preparation: " << preparation_usecs << " usecs" << std::endl;
        }

	std::cout << "num: " << request_num << ", performance: " << request_num * 1000000 / total.restart() << std::endl;

	work.reset();
	service.stop();
	thread.join();

        return 0;
}
