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

#include "networkmanager.h"
#include <list>
#include <iostream>

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
    request.url = argv[1];
    request.want_headers = 1;
    request.headers = {
        { "Content-Type", "text/html; always" },
        { "Additional-Header", "Very long-long\r\n\tsecond line\r\n\tthirdline" }
    };

    manager.get([&loop] (const ioremap::swarm::network_reply &reply) {
        for (auto pair : reply.headers) {
            std::cout << "header: \"" << pair.first << "\": \"" << pair.second << "\"" << std::endl;
        }
        std::cout << "data: " << reply.data << std::endl;

        loop.unloop();
    }, request);

    loop.loop();

    return 0;
}
