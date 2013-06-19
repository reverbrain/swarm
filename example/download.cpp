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

    void operator() (ev::sig &, int)
    {
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

    for (auto signal : { SIGINT, SIGTERM }) {
        sigs.emplace_back(loop);
        ev::sig &sig_watcher = sigs.back();
        sig_watcher.set(signal);
        sig_watcher.set(&shandler);
        sig_watcher.start();
    }

    ioremap::swarm::network_manager manager(loop);

    ioremap::swarm::network_request request;
    request.set_url(argv[1]);
    request.set_follow_location(1);
    request.set_timeout(5000);
    request.set_headers({
        { "Content-Type", "text/html; always" },
        { "Additional-Header", "Very long-long\r\n\tsecond line\r\n\tthird line" }
    });

    typedef std::chrono::high_resolution_clock clock;

    auto begin_time = clock::now();

    manager.get([&loop] (const ioremap::swarm::network_reply &reply) {
        std::cout << "HTTP code: " << reply.get_code() << std::endl;
        std::cout << "Network error: " << reply.get_error() << std::endl;

        for (auto pair : reply.get_headers()) {
            std::cout << "header: \"" << pair.first << "\": \"" << pair.second << "\"" << std::endl;
        }
        std::cout << "data: " << reply.get_data() << std::endl;

        loop.unloop();
    }, request);

    loop.loop();

    auto end_time = clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time);
    std::cout << "Finished in: " << ms.count() << " ms" << std::endl;

    return 0;
}
