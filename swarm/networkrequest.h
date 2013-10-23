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

#ifndef COCAINE_SERVICE_NETWORKREQUEST_H
#define COCAINE_SERVICE_NETWORKREQUEST_H

#include <vector>
#include <string>
#include <utility>
#include "network_url.h"
#include "http_headers.hpp"

#include <boost/optional.hpp>

namespace ioremap {
namespace swarm {

class network_request_data;
class network_reply_data;

typedef std::pair<std::string, std::string> headers_entry;

class http_request
{
public:
	http_request();
	http_request(http_request &&other);
	http_request(const http_request &other);
	~http_request();

	http_request &operator =(http_request &&other);
	http_request &operator =(const http_request &other);

	// Request URL
	const swarm::url &url() const;
	void set_url(const swarm::url &url);
	void set_url(const std::string &url);

	bool is_keep_alive() const;

	// Follow Location from 302 HTTP replies
	bool follow_location() const;
	void set_follow_location(bool follow_location);

	// Timeout in ms
	long timeout() const;
	void set_timeout(long timeout);

	// HTTP headers
	http_headers &headers();
	const http_headers &headers() const;

	// TheVoid specific arguments
	void set_http_version(int major_version, int minor_version);
	int http_major_version() const;
	int http_minor_version() const;

	void set_method(const std::string &method);
	std::string method() const;

private:
	std::unique_ptr<network_request_data> m_data;
};

class http_response
{
public:
	enum status_type {
		continue_code = 100,
		switching_protocols = 101,
		processing = 102,
		ok = 200,
		created = 201,
		accepted = 202,
		non_authoritative_information = 203,
		no_content = 204,
		reset_content = 205,
		partial_content = 206,
		multi_status = 207,
		already_reported = 208,
		im_used = 209,
		multiple_choices = 300,
		moved_permanently = 301,
		moved_temporarily = 302,
		not_modified = 304,
		use_proxy = 305,
		switch_proxy = 306,
		temporary_redirect = 307,
		permanent_redirect = 308,
		bad_request = 400,
		unauthorized = 401,
		payment_required = 402,
		forbidden = 403,
		not_found = 404,
		method_not_allowed = 405,
		not_acceptable = 406,
		proxy_authentication_required = 407,
		request_timeout = 408,
		conflict = 409,
		gone = 410,
		length_required = 411,
		precondition_failed = 412,
		request_entity_too_large = 413,
		request_uri_too_long = 414,
		unsupported_media_type = 415,
		requested_range_not_satisfiable = 416,
		expectation_failed = 417,
		im_a_teapot = 418,
		authentication_timeout = 419,
		unprocessable_entity = 422,
		locked = 423,
		failed_dependency = 424,
		upgrade_required = 426,
		precondition_required = 428,
		too_many_requests = 429,
		request_header_fields_too_large = 431,
		no_response = 444,
		internal_server_error = 500,
		not_implemented = 501,
		bad_gateway = 502,
		service_unavailable = 503,
		gateway_timeout = 504,
		http_version_not_supported = 505,
		variant_also_negotiates = 506,
		insufficient_storage = 507,
		loop_detected = 508,
		not_extended = 510,
		network_authentication_required = 511,
		connection_timed_out = 522
	};

	http_response();
	http_response(http_response &&other);
	http_response(const http_response &other);
	~http_response();

	http_response &operator =(http_response &&other);
	http_response &operator =(const http_response &other);

	// Original request
	const http_request &request() const;
	void set_request(const http_request &request);

	// HTTP code
	int code() const;
	void set_code(int code);

	// Errno
	int error() const;
	void set_error(int error);

	// HTTP headers
	http_headers &headers();
	const http_headers &headers() const;
	void set_headers(const http_headers &headers);
	void set_headers(http_headers &&headers);

	// Final URL from HTTP reply
	const swarm::url &url() const;
	void set_url(const swarm::url &url);
	void set_url(const std::string &url);

	// Reply data
	const std::string &data() const;
	void set_data(const std::string &data);

private:
	std::unique_ptr<network_reply_data> m_data;
};

}
}

#endif // COCAINE_SERVICE_NETWORKREQUEST_H
