/*
 * Copyright 2013+ Ruslan Nigmatullin <euroelessar@yandex.ru>
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

#include "url_fetcher.hpp"
#include "../c++config.hpp"

#include <string.h>
#include <curl/curl.h>

#include <sstream>
#include <iostream>
#include <mutex>

#ifdef SWARM_CSTDATOMIC
#  include <cstdatomic>
#else
#  include <atomic>
#endif

#include <queue>
#include <list>
#include <algorithm>

#ifndef BOOST_SYSTEM_NOEXCEPT
#  define BOOST_SYSTEM_NOEXCEPT
#endif

namespace ioremap {
namespace swarm {

typedef std::chrono::high_resolution_clock clock;

enum http_command {
	GET,
	POST
};

std::atomic_int alive(0);

class network_connection_info
{
public:
	typedef std::unique_ptr<network_connection_info> ptr;

	network_connection_info() : easy(NULL), redirect_count(0), on_headers_called(false)
	{
		//        error[0] = '\0';
	}
	~network_connection_info()
	{
		curl_easy_cleanup(easy);
		//                error[CURL_ERROR_SIZE - 1] = '\0';
	}

	void ensure_headers_sent()
	{
		if (on_headers_called)
			return;

		long code;
		curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &code);
		char *effective_url = NULL;
		curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &effective_url);

		on_headers_called = true;
		reply.set_code(code);
		reply.set_url(effective_url);
		stream->on_headers(std::move(reply));
	}

	CURL *easy;
	swarm::logger logger;
	url_fetcher::response reply;
	std::shared_ptr<base_stream> stream;
	std::string body;
	long redirect_count;
	bool on_headers_called;

	//    char error[CURL_ERROR_SIZE];
};

class network_manager_private : public event_listener
{
public:
	network_manager_private(event_loop &loop) :
		loop(loop), still_running(0), prev_running(0),
		active_connections(0), active_connections_limit(std::numeric_limits<long>::max())
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
		logger.log(SWARM_LOG_DEBUG, "on_socket_event, fd: %d, revent: %d", fd, revent);

		int action = 0;
		if (revent & socket_read)
			action |= CURL_CSELECT_IN;
		if (revent & socket_write)
			action |= CURL_CSELECT_OUT;

		CURLMcode rc;
		do {
			rc = curl_multi_socket_action(multi, fd, action, &still_running);
		} while (rc == CURLM_CALL_MULTI_PERFORM);
		logger.log(SWARM_LOG_DEBUG, "on_socket_event, socket: %d, rc: %d", fd, int(rc));

		check_run_count();
	}

	void on_timer()
	{
		CURLMcode rc;
		do {
			rc = curl_multi_socket_action(multi, CURL_SOCKET_TIMEOUT, 0, &still_running);
		} while (rc == CURLM_CALL_MULTI_PERFORM);
		logger.log(SWARM_LOG_DEBUG, "on_timer, rc: %d", int(rc));

		check_run_count();
	}

	struct request_info
	{
		typedef std::shared_ptr<request_info> ptr;

		request_info() : request(boost::none), begin(clock::now())
		{
		}

		url_fetcher::request request;
		http_command command;
		std::string body;
		std::shared_ptr<base_stream> stream;
		std::chrono::time_point<clock> begin;
	};

	struct multi_error_category : public boost::system::error_category
	{
	public:
		enum {
			failed_to_create_easy_handle = 50
		};

		const char *name() const BOOST_SYSTEM_NOEXCEPT
		{
			return "curl_multi_code";
		}

		std::string message(int ev) const
		{
			if (ev == failed_to_create_easy_handle) {
				return "failed to create easy handle";
			}

			return curl_multi_strerror(static_cast<CURLMcode>(ev));
		}
	};

	static const multi_error_category &multi_category()
	{
		static multi_error_category instance;
		return instance;
	}

	static boost::system::error_code make_posix_error(int err)
	{
		return boost::system::errc::make_error_code(static_cast<boost::system::errc::errc_t>(err));
	}

	static boost::system::error_code make_multi_error(int err)
	{
		return boost::system::error_code(err, multi_category());
	}

	void process_info(const request_info::ptr &request)
	{
		if (active_connections >= active_connections_limit) {
			requests.push(request);
			return;
		}

		process_info_nocheck(request);
	}

	void process_info_nocheck(const request_info::ptr &request)
	{
//		auto tmp = clock::now();

		network_connection_info::ptr info(new network_connection_info);
		info->easy = curl_easy_init();
		info->reply.set_request(std::move(request->request));
		info->reply.set_url(info->reply.request().url());
		info->reply.set_code(200);
		info->stream = request->stream;
		info->body = std::move(request->body);
		info->logger = logger;
		if (!info->easy) {
			info->stream->on_close(make_multi_error(multi_error_category::failed_to_create_easy_handle));
			return;
		}

		struct curl_slist *headers_list = NULL;
		const auto &headers = request->request.headers().all();
		std::string line;
		for (auto it = headers.begin(); it != headers.end(); ++it) {
			line.clear();
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

		curl_easy_setopt(info->easy, CURLOPT_VERBOSE, 0L);
		curl_easy_setopt(info->easy, CURLOPT_URL, info->reply.request().url().to_string().c_str());
		curl_easy_setopt(info->easy, CURLOPT_TIMEOUT_MS, info->reply.request().timeout());
#if LIBCURL_VERSION_NUM >= 0x071507
		/*
		 * If CURL don't support CURLOPT_CLOSESOCKETFUNCTION yet we should fallback
		 * to dup-method to prevent memory leak
		 */
		curl_easy_setopt(info->easy, CURLOPT_OPENSOCKETFUNCTION, network_manager_private::open_callback);
		curl_easy_setopt(info->easy, CURLOPT_OPENSOCKETDATA, &loop);
		curl_easy_setopt(info->easy, CURLOPT_CLOSESOCKETFUNCTION, network_manager_private::close_callback);
		curl_easy_setopt(info->easy, CURLOPT_CLOSESOCKETDATA, &loop);
#endif
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

		if (info->reply.request().follow_location())
			curl_easy_setopt(info->easy, CURLOPT_FOLLOWLOCATION, 1L);

		CURLMcode err = curl_multi_add_handle(multi, info.get()->easy);

//		auto end = clock::now();
//		std::cout << "process_info: " << std::chrono::duration_cast<std::chrono::microseconds>(tmp - request->begin).count() / 1000. << " ms"
//			  << ", add_handle: " << std::chrono::duration_cast<std::chrono::microseconds>(end - tmp).count() / 1000. << " ms"
//			  << std::endl;
		if (err == CURLM_OK) {
			++active_connections;
			/*
			 * We saved info's content in info->easy and stored it in multi handler,
			 * which will free it, so we just forget about info's content here.
			 * Info's destructor (~network_connection_info()) will not be called.
			 */
			info.release();
		} else {
			/*
			 * If exception is being thrown, info will be deleted and easy handler will be destroyed,
			 * which is ok, since easy handler was not added into multi handler in this case.
			 */
			info->stream->on_close(make_multi_error(err));
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
				info->ensure_headers_sent();

				--active_connections;
				if (msg->data.result == CURLE_OPERATION_TIMEDOUT) {
					info->stream->on_close(make_posix_error(ETIMEDOUT));
				} else {
					long err = 0;
					curl_easy_getinfo(easy, CURLINFO_OS_ERRNO, &err);

					if (err) {
						info->stream->on_close(make_posix_error(err));
					} else {
						info->stream->on_close(boost::system::error_code());
					}
				}
			} catch (...) {
				curl_multi_remove_handle(multi, easy);
				delete info;

				throw;
			}

			curl_multi_remove_handle(multi, easy);
			delete info;
		} while (easy);

		while (!requests.empty() && active_connections < active_connections_limit) {
			auto request = requests.front();
			requests.pop();
			process_info_nocheck(request);
		}
	}

	static int open_callback(event_loop *loop, curlsocktype purpose, struct curl_sockaddr *address)
	{
		if (purpose != CURLSOCKTYPE_IPCXN) {
			return CURL_SOCKET_BAD;
		}
		return loop->open_socket(address->family, address->socktype, address->protocol);
	}

	static int close_callback(event_loop *loop, curl_socket_t item)
	{
		return loop->close_socket(item);
	}

	static int socket_callback(CURL *e, curl_socket_t s, int what, url_fetcher *manager, void *data)
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
				manager->p->logger.log(SWARM_LOG_INFO, "socket_callback, unknown what: %d", what);
				return 0;
		}

		return manager->p->loop.socket_request(s, option, data);
	}

	static int timer_callback(CURLM *multi, long timeout_ms, url_fetcher *manager)
	{
		(void) multi;

		return manager->p->loop.timer_request(timeout_ms);
	}

	static size_t write_callback(char *data, size_t size, size_t nmemb, network_connection_info *info)
	{
		info->ensure_headers_sent();
		info->logger.log(SWARM_LOG_DEBUG, "write_callback, size: %zu, nmemb: %zu", size, nmemb);
		const size_t real_size = size * nmemb;
		info->stream->on_data(boost::asio::buffer(data, real_size));
		return real_size;
	}

	template <typename Iter>
	static inline void trim_line(Iter &begin, Iter &end)
	{
		while (begin < end && isspace(*begin))
		    ++begin;
		while (begin < end && isspace(*(end - 1)))
		    --end;
	}

	static std::string trimmed(const char *begin, const char *end)
	{
		trim_line(begin, end);
		return std::string(begin, end);
	}

	static size_t header_callback(char *data, size_t size, size_t nmemb, network_connection_info *info) {
		const size_t real_size = size * nmemb;

		// Empty header line, so it's the end
		long redirect_count;
		curl_easy_getinfo(info->easy, CURLINFO_REDIRECT_COUNT, &redirect_count);

		if (redirect_count != info->redirect_count) {
			info->redirect_count = redirect_count;
			info->reply.headers().clear();
		}

		if (real_size == 2) {
			long code;
			curl_easy_getinfo(info->easy, CURLINFO_RESPONSE_CODE, &code);

			// Check for location header, if it's not redirect - it's surely final request
			if (code < 300
				|| code >= 400
				|| !(info->reply.request().follow_location()
					&& info->reply.headers().has("Location"))) {
				info->ensure_headers_sent();
			}

			return real_size;
		}

		char *lf;
		char *end = data + real_size;
		char *colon = std::find(data, end, ':');

		if (colon != end) {
			std::string field = trimmed(data, colon);
			std::string value;
			value.reserve(16);

			data = colon + 1;

			// any number of LWS is allowed after field, rfc 2068
			do {
				lf = std::find(data, end, '\n');

				if (!value.empty())
					value += ' ';

				auto value_begin = data;
				auto value_end = lf;
				trim_line(value_begin, value_end);
				value.append(value_begin, value_end);

				data = lf;
			} while (data < end && (*(data + 1) == ' ' || *(data + 1) == '\t'));

			swarm::headers_entry entry = { std::move(field), std::move(value) };
			info->reply.headers().add(std::move(entry));
		}

		return size * nmemb;
	}

	event_loop &loop;
	int still_running;
	int prev_running;
	std::atomic_long active_connections;
	long active_connections_limit;
	std::queue<request_info::ptr, std::list<request_info::ptr>> requests;
	swarm::logger logger;
	CURLM *multi;
};

url_fetcher::url_fetcher(event_loop &loop, const swarm::logger &logger)
	: p(new network_manager_private(loop))
{
	p->logger = logger;
	p->loop.set_logger(logger);
	p->logger.log(SWARM_LOG_INFO, "Creating network_manager: %p", this);
	p->multi = curl_multi_init();
	curl_multi_setopt(p->multi, CURLMOPT_SOCKETFUNCTION, network_manager_private::socket_callback);
	curl_multi_setopt(p->multi, CURLMOPT_SOCKETDATA, this);
	curl_multi_setopt(p->multi, CURLMOPT_TIMERFUNCTION, network_manager_private::timer_callback);
	curl_multi_setopt(p->multi, CURLMOPT_TIMERDATA, this);
	curl_multi_setopt(p->multi, CURLMOPT_PIPELINING, long(1));
}

url_fetcher::~url_fetcher()
{
	p->logger.log(SWARM_LOG_INFO, "Destroying network_manager: %p", this);
	delete p;
}

void url_fetcher::set_total_limit(long active_connections)
{
	p->active_connections_limit = active_connections;
}

void url_fetcher::set_logger(const swarm::logger &log)
{
	p->loop.set_logger(log);
	p->logger = log;
}

swarm::logger url_fetcher::logger() const
{
	return p->logger;
}

void url_fetcher::get(const std::shared_ptr<base_stream> &stream, url_fetcher::request &&request)
{
	auto info = std::make_shared<network_manager_private::request_info>();
	info->stream = stream;
	info->request = std::move(request);
	info->command = GET;

	p->loop.post(std::bind(&network_manager_private::process_info, p, info));
}

void url_fetcher::post(const std::shared_ptr<base_stream> &stream, url_fetcher::request &&request, std::string &&body)
{
	auto info = std::make_shared<network_manager_private::request_info>();
	info->stream = stream;
	info->request = std::move(request);
	info->command = POST;
	info->body = std::move(body);

	p->loop.post(std::bind(&network_manager_private::process_info, p, info));
}

class url_fetcher_request_data
{
public:
	url_fetcher_request_data() : follow_location(false), timeout(30000)
	{
	}

	bool follow_location;
	long timeout;
};

class url_fetcher_response_data
{
public:
	url_fetcher_response_data() : request(boost::none)
	{
	}

	swarm::url url;
	url_fetcher::request request;
};

url_fetcher::request::request() : m_data(new url_fetcher_request_data)
{
}

url_fetcher::request::request(const boost::none_t &none) : http_request(none)
{

}

url_fetcher::request::request(url_fetcher::request &&other) :
	http_request(other), m_data(std::move(other.m_data))
{
}

url_fetcher::request::request(const url_fetcher::request &other) :
	http_request(other), m_data(new url_fetcher_request_data(*other.m_data))
{
}

url_fetcher::request::~request()
{
}

url_fetcher::request &url_fetcher::request::operator =(url_fetcher::request &&other)
{
	m_data = std::move(other.m_data);
	http_request::operator =(other);
	return *this;
}

url_fetcher::request &url_fetcher::request::operator =(const url_fetcher::request &other)
{
	using std::swap;
	url_fetcher::request tmp(other);
	swap(m_data, tmp.m_data);
	http_request::operator =(other);
	return *this;
}

bool url_fetcher::request::follow_location() const
{
	return m_data->follow_location;
}

void url_fetcher::request::set_follow_location(bool follow_location)
{
	m_data->follow_location = follow_location;
}

long url_fetcher::request::timeout() const
{
	return m_data->timeout;
}

void url_fetcher::request::set_timeout(long timeout)
{
	m_data->timeout = timeout;
}

url_fetcher::response::response() : m_data(new url_fetcher_response_data)
{
}

url_fetcher::response::response(const boost::none_t &none) : http_response(none)
{
}

url_fetcher::response::response(url_fetcher::response &&other) :
	http_response(other), m_data(std::move(other.m_data))
{
}

url_fetcher::response::response(const url_fetcher::response &other) :
	http_response(other), m_data(new url_fetcher_response_data(*other.m_data))
{
}

url_fetcher::response::~response()
{
}

url_fetcher::response &url_fetcher::response::operator =(url_fetcher::response &&other)
{
	m_data = std::move(other.m_data);
	http_response::operator =(other);
	return *this;
}

url_fetcher::response &url_fetcher::response::operator =(const url_fetcher::response &other)
{
	using std::swap;
	url_fetcher::response tmp(other);
	swap(m_data, tmp.m_data);
	http_response::operator =(other);
	return *this;
}

const url &url_fetcher::response::url() const
{
	return m_data->url;
}

void url_fetcher::response::set_url(const swarm::url &url)
{
	m_data->url = url;
}

void url_fetcher::response::set_url(const std::string &url)
{
	m_data->url = std::move(swarm::url(url));
}

const url_fetcher::request &url_fetcher::response::request() const
{
	return m_data->request;
}

void url_fetcher::response::set_request(const url_fetcher::request &request)
{
	m_data->request = request;
}

void url_fetcher::response::set_request(url_fetcher::request &&request)
{
	m_data->request = std::move(request);
}

} // namespace service
} // namespace cocaine
