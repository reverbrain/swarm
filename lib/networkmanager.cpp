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

#include <curl/curl.h>

#include <sstream>
#include <iostream>
#include <mutex>
#include <atomic>
#include <list>

#define debug() if (1) {} else std::cerr << __PRETTY_FUNCTION__ << ": " << __LINE__ << " "

namespace ioremap {
namespace swarm {

static std::atomic_size_t counter;

class network_connection_info
{
public:
    typedef std::unique_ptr<network_connection_info> ptr;

    network_connection_info() : easy(NULL)
    {
        ++counter;
//        error[0] = '\0';
    }
    ~network_connection_info()
    {
//        std::cerr << "~network_connection_info: " << --counter << std::endl;
        curl_easy_cleanup(easy);
//        error[CURL_ERROR_SIZE - 1] = '\0';
    }

    CURL *easy;
    network_reply reply;
    std::function<void (const network_reply &reply)> handler;
    std::stringstream data;
//    char error[CURL_ERROR_SIZE];
};

class network_manager_private
{
public:
    network_manager_private(ev::loop_ref &loop)
        : loop(loop), timer(loop), async(loop), still_running(0),
          prev_running(0), active_connections_limit(10), active_connections(0)
    {
        timer.set<network_manager_private, &network_manager_private::on_timer>(this);
        async.set<network_manager_private, &network_manager_private::on_async>(this);
        async.start();
    }

    void on_socket_event(ev::io &io, int revent)
    {
        debug() << std::endl;
        int action = 0;
        if (revent & EV_READ)
            action |= CURL_CSELECT_IN;
        if (revent & EV_WRITE)
            action |= CURL_CSELECT_OUT;

        CURLMcode rc;
        do {
            rc = curl_multi_socket_action(multi, io.fd, action, &still_running);
        } while (rc == CURLM_CALL_MULTI_PERFORM);
        debug() << " " << rc << std::endl;

        check_run_count();
    }

    void on_timer(ev::timer &, int)
    {
        debug() << std::endl;
        CURLMcode rc;
        do {
            rc = curl_multi_socket_action(multi,
                                          CURL_SOCKET_TIMEOUT, 0, &still_running);
        } while (rc == CURLM_CALL_MULTI_PERFORM);
        debug() << rc << std::endl;

        check_run_count();
    }

    void on_async(ev::async &, int)
    {
        next_connections();
    }

    void next_connections()
    {
        while (active_connections < active_connections_limit) {
            request_info::ptr request;

            {
                std::lock_guard<std::mutex> lock(mutex);
                if (requests.empty())
                    break;
                request = requests.front();
                requests.erase(requests.begin());
            }

            network_connection_info::ptr info(new network_connection_info);
            info->easy = curl_easy_init();
            info->reply.request = request->request;
            info->reply.url = request->request.url;
            info->reply.code = 200;
            info->handler = request->handler;
            if (!info->easy) {
                info->reply.code = 650;
                request->handler(info->reply);
                continue;
            }

//            struct curl_slist *headers_list = NULL;
//            for (auto &header : request->request.headers)
//                headers_list = curl_slist_append(headers_list, header.c_str());

            //    curl_easy_setopt(info->easy, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(info->easy, CURLOPT_URL, info->reply.request.url.c_str());
            curl_easy_setopt(info->easy, CURLOPT_WRITEFUNCTION, network_manager_private::write_callback);

	    /*
	     * Grab raw data and free it later in curl_easy_cleanup()
	     */
            curl_easy_setopt(info->easy, CURLOPT_WRITEDATA, info.get());
//            curl_easy_setopt(info->easy, CURLOPT_ERRORBUFFER, info->error);
            curl_easy_setopt(info->easy, CURLOPT_PRIVATE, info.get());

            if (request->request.follow_location)
                curl_easy_setopt(info->easy, CURLOPT_FOLLOWLOCATION, 1L);

            CURLMcode err = curl_multi_add_handle(multi, info.get()->easy);
            if (err == CURLM_OK) {
                ++active_connections;
                /*
	         * We saved info's content in info->easy and stored it in multi handler,
	         * which will free it, so we just forget about info's content here.
	         * Info's destructor (~network_connection_info()) will not be called.
	         */
                info.release();
            } else {
                info->reply.code = 600 + err;
		/*
		 * If exception is being thrown, info will be deleted and easy handler will be destroyed,
		 * which is ok, since easy handler was not added into multi handler in this case.
		 */
                info->handler(info->reply);
            }
        }
    }

    /* Check for completed transfers, and remove their easy handles */
    void check_run_count()
    {
        debug() << prev_running << " " << still_running << std::endl;
        if (prev_running > still_running) {
            char *effective_url = NULL;
            CURLMsg *msg;
            int messsages_left;
            network_connection_info *info = NULL;
            CURL*easy;
            CURLcode res;

            /*
	     * I am still uncertain whether it is safe to remove an easy handle
	     * from inside the curl_multi_info_read loop, so here I will search
	     * for completed transfers in the inner "while" loop, and then remove
	     * them in the outer "do-while" loop...
	     */

            do {
                easy = NULL;
                while ((msg = curl_multi_info_read(multi, &messsages_left))) {
                    if (msg->msg == CURLMSG_DONE) {
                        easy = msg->easy_handle;
                        res = msg->data.result;
                        break;
                    }
                }

                if (!easy)
		    break;

                curl_easy_getinfo(easy, CURLINFO_PRIVATE, &info);
                curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &effective_url);

                try {
                    --active_connections;
                    long code = 200;
                    long err = 0;
                    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &code);
                    curl_easy_getinfo(easy, CURLINFO_OS_ERRNO, &err);
                    info->reply.code = code;
                    info->reply.error = -err;
                    info->reply.url = effective_url;
                    info->reply.data = info->data.str();
                    info->handler(info->reply);
                } catch (...) {
                    curl_multi_remove_handle(multi, easy);
                    delete info;

		    throw;
                }

                curl_multi_remove_handle(multi, easy);
                delete info;
            } while (easy);

            next_connections();
        }
        prev_running = still_running;
    }

    static int socket_callback(CURL *e, curl_socket_t s, int what, network_manager *manager, ev::io *io)
    {
        debug() << std::endl;
        (void) e;

        if (what == CURL_POLL_REMOVE) {
            delete io;
            return 0;
        }

        if (!io) {
            debug() << std::endl;
            io = new ev::io(manager->p->loop);
            curl_multi_assign(manager->p->multi, s, io);
            io->set<network_manager_private, &network_manager_private::on_socket_event>(manager->p);
        }

        int events = 0;
        if (what & CURL_POLL_IN)
            events |= EV_READ;
        if (what & CURL_POLL_OUT)
            events |= EV_WRITE;
        debug() << what << " -> " << events << std::endl;
        bool active = io->is_active();
        io->set(s, events);
        if (!active)
            io->start();
        return 0;
    }

    static int timer_callback(CURLM *multi, long timeout_ms, network_manager *manager)
    {
        debug() << std::endl;
        (void) multi;
        manager->p->timer.stop();
        manager->p->timer.set(timeout_ms / 1000.);
        manager->p->timer.start();
        return 0;
    }

    static size_t write_callback(void *ptr, size_t size, size_t nmemb, network_connection_info *info)
    {
        debug() << std::endl;
        size_t realsize = size * nmemb;
        info->data.write(reinterpret_cast<char*>(ptr), realsize);
        return realsize;
    }

    struct request_info
    {
        typedef std::shared_ptr<request_info> ptr;

        network_request request;
        std::function<void (const network_reply &reply)> handler;
    };

    ev::loop_ref &loop;
    ev::timer timer;
    ev::async async;
    int still_running;
    int prev_running;
    int active_connections_limit;
    std::atomic_int active_connections;
    std::list<request_info::ptr> requests;
    std::mutex mutex;
    CURLM *multi;
};

network_manager::network_manager(ev::loop_ref &loop)
    : p(new network_manager_private(loop))
{
    p->multi = curl_multi_init();
    curl_multi_setopt(p->multi, CURLMOPT_SOCKETFUNCTION, network_manager_private::socket_callback);
    curl_multi_setopt(p->multi, CURLMOPT_SOCKETDATA, this);
    curl_multi_setopt(p->multi, CURLMOPT_TIMERFUNCTION, network_manager_private::timer_callback);
    curl_multi_setopt(p->multi, CURLMOPT_TIMERDATA, this);
}

network_manager::~network_manager()
{
    delete p;
}

void network_manager::set_limit(int active_connections)
{
    p->active_connections_limit = active_connections;
}

void network_manager::get(const std::function<void (const network_reply &reply)> &handler, const network_request &request)
{
    auto info = std::make_shared<network_manager_private::request_info>();
    info->handler = handler;
    info->request = request;

    std::lock_guard<std::mutex> lock(p->mutex);
    p->requests.push_back(info);
    p->async.send();
}

} // namespace service
} // namespace cocaine
