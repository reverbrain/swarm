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

#ifndef IOREMAP_SWARM_HTTP_RESPONSE_HPP
#define IOREMAP_SWARM_HTTP_RESPONSE_HPP

#include <swarm/url.hpp>
#include <swarm/http_headers.hpp>

namespace ioremap {
namespace swarm {

class http_response_data;

/*!
 * \brief The http_response class is a convient API for http responses.
 *
 * It provides read/write access to all HTTP-specific properties like headers,
 * result code and reason.
 *
 * \attention http_response supports move semantics, so it's cheaper to move it if
 * it's possible.
 *
 * \sa http_request
 */
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
	http_response(http_response_data &data);
	http_response(const boost::none_t &);
	http_response(http_response &&other);
	http_response(const http_response &other);
	~http_response();

	http_response &operator =(http_response &&other);
	http_response &operator =(const http_response &other);

	// HTTP code
	int code() const;
	void set_code(int code);

	// HTTP Reason phrase
	boost::optional<std::string> reason() const;
	void set_reason(const std::string &reason);

	// HTTP headers
	http_headers &headers();
	const http_headers &headers() const;
	void set_headers(const http_headers &headers);
	void set_headers(http_headers &&headers);

protected:
	std::unique_ptr<http_response_data> m_data;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_HTTP_RESPONSE_HPP
