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
#include <swarm/c++config.hpp>
#include <list>
#include <iostream>
#include <chrono>
#include <thread>

#include <iostream>

#include <boost/program_options.hpp>

#include "timer.hpp"

using namespace ioremap;

struct request_handler_shared {
};

struct request_handler_functor {
	ev::loop_ref &loop;
	std::atomic_long &counter;
	long total;

	void operator() (const ioremap::swarm::url_fetcher::response &reply, const std::string &data, const boost::system::error_code &error) {
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
		++counter;
		if (counter == total)
			loop.unloop();
	}
};


int main(int argc, char *argv[])
{
        namespace bpo = boost::program_options;

        bpo::options_description generic("Cocaine-service testing options");

        std::string url;

        long request_num, chunk_num;

        generic.add_options()
                ("help", "This help message")
                ("url", bpo::value<std::string>(&url)->default_value("http://localhost:8080/get"), "Test URL for GET request")
                ("requests", bpo::value<long>(&request_num)->default_value(100000), "Number of test calls")
                ("chunk", bpo::value<long>(&chunk_num)->default_value(1000), "Send this many requests and then synchronously wait for all of them to complete")
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

	ev::default_loop loop;
	std::unique_ptr<ioremap::swarm::event_loop> loop_impl;
	loop_impl.reset(new ioremap::swarm::ev_event_loop(loop));

	ioremap::swarm::logger logger("/dev/stdout", ioremap::swarm::SWARM_LOG_ERROR);

	ioremap::swarm::url_fetcher manager(*loop_impl, logger);

	ioremap::warp::timer tm, total;

        for (long i = 0; i < request_num;) {
		std::atomic_long counter(0);
		request_handler_functor request_handler = { loop, counter, chunk_num};

		for (long j = 0; j < chunk_num && i < request_num; ++i, ++j) {
			ioremap::swarm::url_fetcher::request request;
			request.set_url(url);
			request.set_timeout(500000);

			manager.get(ioremap::swarm::simple_stream::create(request_handler), std::move(request));
		}

		loop.loop();
		std::cout << "num: " << chunk_num << ", performance: " << chunk_num * 1000 / tm.restart() << std::endl;
        }

        std::cout << "num: " << request_num << ", performance: " << request_num * 1000 / total.restart() << std::endl;

        return 0;
}
