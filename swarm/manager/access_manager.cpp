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

#include "access_manager.hpp"

#include <string.h>
#include <curl/curl.h>

#include <sstream>
#include <iostream>
#include <mutex>
#if __GNUC__ == 4 && __GNUC_MINOR__ < 5
#  include <cstdatomic>
#else
#  include <atomic>
#endif
#include <list>
#include <algorithm>

namespace ioremap {
namespace swarm {

enum http_command {
    GET,
    POST
};

std::atomic_int alive(0);

class network_connection_info
{
public:
    typedef std::unique_ptr<network_connection_info> ptr;

    network_connection_info() : easy(NULL)
    {
//        error[0] = '\0';
    }
    ~network_connection_info()
    {
        curl_easy_cleanup(easy);
//                error[CURL_ERROR_SIZE - 1] = '\0';
    }

    CURL *easy;
    swarm::logger logger;
    http_response reply;
    std::function<void (const http_response &reply)> handler;
    std::string body;
    std::stringstream data;
//    char error[CURL_ERROR_SIZE];
};

class network_manager_private : public event_listener
{
public:
	network_manager_private(event_loop &loop) :
		loop(loop), still_running(0), prev_running(0),
		active_connections_limit(10), active_connections(0)
	{
		loop.set_listener(this);
		loop.set_logger(logger);
	}

	void set_socket_data(int socket, void *data)
	{
		curl_multi_assign(multi, socket, data);
	}

	void on_socket_event(int fd, int revent)
	{
		logger.log(LOG_DEBUG, "on_socket_event, fd: %d, revent: %d", fd, revent);

		int action = 0;
		if (revent & socket_read)
			action |= CURL_CSELECT_IN;
		if (revent & socket_write)
			action |= CURL_CSELECT_OUT;

		CURLMcode rc;
		do {
			rc = curl_multi_socket_action(multi, fd, action, &still_running);
		} while (rc == CURLM_CALL_MULTI_PERFORM);
		logger.log(LOG_DEBUG, "on_socket_event, socket: %d, rc: %d", fd, int(rc));

		check_run_count();
	}

	void on_timer()
	{
		CURLMcode rc;
		do {
			rc = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, &still_running);
		} while (rc == CURLM_CALL_MULTI_PERFORM);
		logger.log(LOG_DEBUG, "on_timer, rc: %d", int(rc));

		check_run_count();
	}

	struct request_info
        {
            typedef std::shared_ptr<request_info> ptr;

            http_request request;
            http_command command;
            std::string body;
            std::function<void (const http_response &reply)> handler;
        };

    void process_info(request_info::ptr request)
    {
            network_connection_info::ptr info(new network_connection_info);
            info->easy = curl_easy_init();
            info->reply.set_request(request->request);
            info->reply.set_url(request->request.url());
            info->reply.set_code(200);
            info->handler = request->handler;
            info->body = request->body;
            info->logger = logger;
            if (!info->easy) {
                info->reply.set_code(650);
                request->handler(info->reply);
		return;
            }

            struct curl_slist *headers_list = NULL;
	    const auto &headers = request->request.headers().all();
            for (auto it = headers.begin(); it != headers.end(); ++it) {
                std::string line;
                line.reserve(it->first.size() + 2 + it->second.size());
                line += it->first;
                line += ": ";
                line += it->second;

                headers_list = curl_slist_append(headers_list, line.c_str());
            }

            if (request->command == POST) {
                curl_easy_setopt(info->easy, CURLOPT_POST, true);
                curl_easy_setopt(info->easy, CURLOPT_POSTFIELDS, info->body.c_str());
                curl_easy_setopt(info->easy, CURLOPT_POSTFIELDSIZE, info->body.size());
            }

            curl_easy_setopt(info->easy, CURLOPT_HTTPHEADER, headers_list);

//	    curl_easy_setopt(info->easy, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(info->easy, CURLOPT_URL, info->reply.request().url().to_string().c_str());
            curl_easy_setopt(info->easy, CURLOPT_TIMEOUT_MS, info->reply.request().timeout());
            curl_easy_setopt(info->easy, CURLOPT_WRITEFUNCTION, network_manager_private::write_callback);
            curl_easy_setopt(info->easy, CURLOPT_HEADERFUNCTION, network_manager_private::header_callback);
            curl_easy_setopt(info->easy, CURLOPT_HEADERDATA, info.get());
            curl_easy_setopt(info->easy, CURLOPT_NOSIGNAL, 1L);
//            curl_easy_setopt(info->easy, CURLOPT_ERRORBUFFER, info->error);

            /*
             * Grab raw data and free it later in curl_easy_cleanup()
             */
            curl_easy_setopt(info->easy, CURLOPT_WRITEDATA, info.get());
            curl_easy_setopt(info->easy, CURLOPT_PRIVATE, info.get());

            if (request->request.follow_location())
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
                info->reply.set_code(600 + err);
                /*
                 * If exception is being thrown, info will be deleted and easy handler will be destroyed,
                 * which is ok, since easy handler was not added into multi handler in this case.
                 */
                info->handler(info->reply);
            }
    }

    /* Check for completed transfers, and remove their easy handles */
    void check_run_count()
    {
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
				    (void) res;
				    break;
			    }
		    }

		    if (!easy)
			    break;

		    curl_easy_getinfo(easy, CURLINFO_PRIVATE, &info);
		    curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &effective_url);

		    try {
			    --active_connections;
			    if (msg->data.result == CURLE_OPERATION_TIMEDOUT) {
				    info->reply.set_code(0);
				    info->reply.set_error(-ETIMEDOUT);
			    } else {
				    long code = 200;
				    long err = 0;
				    curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &code);
				    curl_easy_getinfo(easy, CURLINFO_OS_ERRNO, &err);
				    info->reply.set_code(code);
				    info->reply.set_error(-err);
			    }
			    info->reply.set_url(effective_url);
			    info->reply.set_data(info->data.str());
			    info->handler(info->reply);
		    } catch (...) {
			    curl_multi_remove_handle(multi, easy);
			    delete info;

			    throw;
		    }

		    curl_multi_remove_handle(multi, easy);
		    delete info;
	    } while (easy);
    }

    static int socket_callback(CURL *e, curl_socket_t s, int what, access_manager *manager, void *data)
    {
	    (void) e;

	    event_loop::poll_option option;

	    switch (what) {
		    case CURL_POLL_REMOVE:
			    option = event_loop::poll_remove;
			    break;
		    case CURL_POLL_IN:
			    option = event_loop::poll_in;
			    break;
		    case CURL_POLL_OUT:
			    option = event_loop::poll_out;
			    break;
		    case CURL_POLL_INOUT:
			    option = event_loop::poll_all;
			    break;
		    default:
			    manager->p->logger.log(LOG_INFO, "socket_callback, unknown what: %d", what);
			    return 0;
	    }

	    return manager->p->loop.socket_request(s, option, data);
    }

    static int timer_callback(CURLM *multi, long timeout_ms, access_manager *manager)
    {
	    (void) multi;

	    return manager->p->loop.timer_request(timeout_ms);
    }

    static size_t write_callback(char *data, size_t size, size_t nmemb, network_connection_info *info)
    {
	    info->logger.log(LOG_DEBUG, "write_callback, size: %zu, nmemb: %zu", size, nmemb);
	    const size_t real_size = size * nmemb;
	    info->data.write(data, real_size);
	    return real_size;
    }

    static std::string trimmed(const char *begin, const char *end)
    {
        while (begin < end && isspace(*begin))
            ++begin;
        while (begin < end && isspace(*(end - 1)))
            --end;

        return std::string(begin, end);
    }

    static size_t header_callback(char *data, size_t size, size_t nmemb, network_connection_info *info) {
        const size_t real_size = size * nmemb;

        char *lf;
        char *end = data + real_size;
        char *colon = std::find(data, end, ':');

        if (colon != end) {
            const std::string field = trimmed(data, colon);
            std::string value;

            data = colon + 1;

            // any number of LWS is allowed after field, rfc 2068
            do {
                lf = std::find(data, end, '\n');

                if (!value.empty())
                    value += ' ';

                value += trimmed(data, lf);

                data = lf;
            } while (data < end && (*(data + 1) == ' ' || *(data + 1) == '\t'));

            info->reply.headers().add(field, value);
        }

        return size * nmemb;
    }

    event_loop &loop;
    int still_running;
    int prev_running;
    int active_connections_limit;
    std::atomic_int active_connections;
    std::list<request_info::ptr> requests;
    swarm::logger logger;
    CURLM *multi;
};

access_manager::access_manager(event_loop &loop, const swarm::logger &logger)
	: p(new network_manager_private(loop))
{
	p->logger = logger;
	p->loop.set_logger(logger);
	p->logger.log(LOG_INFO, "Creating network_manager: %p", this);
	p->multi = curl_multi_init();
        curl_multi_setopt(p->multi, CURLMOPT_SOCKETFUNCTION, network_manager_private::socket_callback);
        curl_multi_setopt(p->multi, CURLMOPT_SOCKETDATA, this);
        curl_multi_setopt(p->multi, CURLMOPT_TIMERFUNCTION, network_manager_private::timer_callback);
	curl_multi_setopt(p->multi, CURLMOPT_TIMERDATA, this);
}

access_manager::~access_manager()
{
	p->logger.log(LOG_INFO, "Destroying network_manager: %p", this);
	delete p;
}

void access_manager::set_limit(int active_connections)
{
	p->active_connections_limit = active_connections;
}

void access_manager::set_logger(const swarm::logger &log)
{
	p->loop.set_logger(log);
	p->logger = log;
}

swarm::logger access_manager::logger() const
{
	return p->logger;
}

void access_manager::get(const std::function<void (const http_response &reply)> &handler, const http_request &request)
{
    auto info = std::make_shared<network_manager_private::request_info>();
    info->handler = handler;
    info->request = request;
    info->command = GET;

    p->loop.post(std::bind(&network_manager_private::process_info, p, info));
}

void access_manager::post(const std::function<void (const http_response &)> &handler, const http_request &request, const std::string &body)
{
    auto info = std::make_shared<network_manager_private::request_info>();
    info->handler = handler;
    info->request = request;
    info->command = POST;
    info->body = body;

    p->loop.post(std::bind(&network_manager_private::process_info, p, info));
}

} // namespace service
} // namespace cocaine
