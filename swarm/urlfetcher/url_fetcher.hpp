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

#ifndef COCAINE_SERVICE_NETWORKMANAGER_H
#define COCAINE_SERVICE_NETWORKMANAGER_H

#include "../http_response.hpp"
#include "../http_request.hpp"

#include "../logger.hpp"
#include "event_loop.hpp"
#include <memory>
#include <functional>
#include <map>

#include <boost/asio.hpp>
#include <boost/variant.hpp>

namespace ioremap {
namespace swarm {

class network_manager_private;
class url_fetcher_request_data;
class url_fetcher_response_data;
class base_stream;

/*!
 * \class url_fetcher
 * \brief The url_fetcher class provides convient API for fetching HTTP urls.
 *
 * Url fetcher provides API for performing GET and POST requests to remote HTTP servers
 * by methods get and post.
 *
 * Url fetcher uses provided class event_loop as an event loop for polling i/o operations.
 *
 * \attention Url fetcher's event loop must be executed exactly in one thread. Multithreading
 * is \b unsupported. To run url fetcher in different threads create different instances.
 *
 * Sometimes you may want to do synchronous requests to url fetcher. In this case you need to
 * use std::condition_variable to wait for request being processed.
 *
 * All methods except get and post are not thread safe.
 */
class url_fetcher
{
public:
	url_fetcher();
	/*!
	 * \brief Constructs Url Fetcher with \a loop and \a logger.
	 */
	url_fetcher(event_loop &loop, const swarm::logger &logger);
	url_fetcher(url_fetcher &&other);
	~url_fetcher();

	url_fetcher &operator =(url_fetcher &&other);

	class request : public http_request
	{
		typedef url_fetcher_request_data data;
	public:
		request();
		request(const boost::none_t &);
		request(request &&other);
		request(const request &other);
		~request();

		request &operator =(request &&other);
		request &operator =(const request &other);

		bool follow_location() const;
		/*!
		 * \brief Makes request to \a follow_location.
		 *
		 * If this variable is true url fetcher will resend the request to
		 * the value of Location header in case if HTTP code is 3xx.
		 */
		void set_follow_location(bool follow_location);

		long timeout() const;
		/*!
		 * \brief Sets \a timeout milliseconds as the request timeout.
		 *
		 * If requests performs more than \a timeout ms it will be aborted with error code 110.
		 */
		void set_timeout(long timeout);

		bool verify_ssl_peers() const;
		/*!
		 * \brief Specifies if the client verifies the SSL peers. True by default.
		 *
		 * If verify is true (the default) and the SSL certificate of the peer is self-signed
		 * then the request fails. A value of false allows for self-signed certificates.
		 */
		void set_verify_ssl_peers(bool verify);
	};

	class response : public http_response
	{
		typedef url_fetcher_response_data data;
	public:
		response();
		response(const boost::none_t &);
		response(response &&other);
		response(const response &other);
		~response();

		response &operator =(response &&other);
		response &operator =(const response &other);

		/*!
		 * \brief Returnes the final url answer was received from.
		 *
		 * It may be different from ioremap::swarm::http_request::url in case if
		 * request::follow_location was set to true.
		 */
		const swarm::url &url() const;
		void set_url(const swarm::url &url);
		void set_url(const std::string &url);

		/*!
		 * \brief Returnes the original request for this response.
		 */
		const url_fetcher::request &request() const;
		void set_request(const url_fetcher::request &request);
		void set_request(url_fetcher::request &&request);
	};

	/*!
	 * \brief Set limit of simultaneously running requests to \a active_connections.
	 *
	 * Processing of too many concurrent requests may lead to increadibly worse performance.
	 *
	 * By default this property is set to LONG_MAX.
	 */
	void set_total_limit(long active_connections);

	const swarm::logger &logger() const;

	/*!
	 * \brief Make GET HTTP request to server by \a request. Result will be send to \a stream.
	 *
	 * This method is asynchronious, so any information such as received headers/data will
	 * invoke appropriate methods of \a stream.
	 *
	 * This method is thread safe.
	 *
	 * Shared pointer to \a stream will be destroyed once the request is finished.
	 *
	 * \sa post
	 */
	void get(const std::shared_ptr<base_stream> &stream, url_fetcher::request &&request);
	/*!
	 * \brief Make POST HTTP request to server by \a request with \a body. Result will be send to \a stream.
	 *
	 * This method is asynchronious, so any information such as received headers/data will
	 * invoke appropriate methods of \a stream.
	 *
	 * This method is thread safe.
	 *
	 * Shared pointer to \a stream will be destroyed once the request is finished.
	 *
	 * \sa get
	 */
	void post(const std::shared_ptr<base_stream> &stream, url_fetcher::request &&request, std::string &&body);

private:
	url_fetcher(const url_fetcher &other) = delete;
	url_fetcher &operator =(const url_fetcher &other) = delete;

	network_manager_private *p;

	friend class network_manager_private;
};

/*!
 * \brief The base_stream class is an interface for handling request-specific events.
 */
class base_stream
{
public:
	/*!
	 * \brief Destroyes the base_stream.
	 */
	virtual ~base_stream() {}

	/*!
	 * \brief This method is called once final headers are received.
	 *
	 * \attention In case of relocations by 3xx code this method is not called
	 * until reply for final request is received.
	 */
	virtual void on_headers(url_fetcher::response &&response) = 0;
	/*!
	 * \brief This method is called on every chunk recevied from the server.
	 *
	 * This method may be called more than once if server's reply doesn't fit
	 * the internal buffer or the connection is slow.
	 */
	virtual void on_data(const boost::asio::const_buffer &data) = 0;
	/*!
	 * \brief This method is called when the request is finished either because
	 * of the error or if all data from the server are received.
	 *
	 * Possible errors are all from boost::asio and in addition following categories:
	 * \li "curl_multi_code" - errors from enum CURLMcode
	 * \li "curl_easy_code" - errors from enum CURLcode
	 *
	 * So i.e. timeout is notified by "curl_easy_code" error category and CURLE_OPERATION_TIMEDOUT error.
	 */
	virtual void on_close(const boost::system::error_code &error) = 0;
};

} // namespace service
} // namespace cocaine

#endif // COCAINE_SERVICE_NETWORKMANAGER_H
