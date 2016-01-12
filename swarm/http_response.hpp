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

#include "url.hpp"
#include "http_headers.hpp"

#include <boost/asio/buffer.hpp>

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
		im_used = 226,
		multiple_choices = 300,
		moved_permanently = 301,
		moved_temporarily = 302,
		found = 302,
		see_other = 303,
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
		connection_timed_out = 522,

		HTTP_100_CONTINUE = 100,
		HTTP_101_SWITCHING_PROTOCOLS = 101,
		HTTP_102_PROCESSING = 102,

		HTTP_200_OK = 200,
		HTTP_201_CREATED = 201,
		HTTP_202_ACCEPTED = 202,
		HTTP_203_NON_AUTHORITATIVE_INFORMATION = 203,
		HTTP_204_NO_CONTENT = 204,
		HTTP_205_RESET_CONTENT = 205,
		HTTP_206_PARTIAL_CONTENT = 206,
		HTTP_207_MULTI_STATUS = 207,
		HTTP_208_ALREADY_REPORTED = 208,
		HTTP_226_IM_USED = 226,

		HTTP_300_MULTIPLE_CHOICES = 300,
		HTTP_301_MOVED_PERMANENTLY = 301,
		HTTP_302_MOVED_TEMPORARILY = 302,
		HTTP_302_FOUND = 302,
		HTTP_303_SEE_OTHER = 303,
		HTTP_304_NOT_MODIFIED = 304,
		HTTP_305_USE_PROXY = 305,
		HTTP_306_SWITCH_PROXY = 306,
		HTTP_307_TEMPORARY_REDIRECT = 307,
		HTTP_308_PERMANENT_REDIRECT = 308,

		HTTP_400_BAD_REQUEST = 400,
		HTTP_401_UNAUTHORIZED = 401,
		HTTP_402_PAYMENT_REQUIRED = 402,
		HTTP_403_FORBIDDEN = 403,
		HTTP_404_NOT_FOUND = 404,
		HTTP_405_METHOD_NOT_ALLOWED = 405,
		HTTP_406_NOT_ACCEPTABLE = 406,
		HTTP_407_PROXY_AUTHENTICATION_REQUIRED = 407,
		HTTP_408_REQUEST_TIMEOUT = 408,
		HTTP_409_CONFLICT = 409,
		HTTP_410_GONE = 410,
		HTTP_411_LENGTH_REQUIRED = 411,
		HTTP_412_PRECONDITION_FAILED = 412,
		HTTP_413_REQUEST_ENTITY_TOO_LARGE = 413,
		HTTP_414_REQUEST_URI_TOO_LONG = 414,
		HTTP_415_UNSUPPORTED_MEDIA_TYPE = 415,
		HTTP_416_REQUESTED_RANGE_NOT_SATISFIABLE = 416,
		HTTP_417_EXPECTATION_FAILED = 417,
		HTTP_418_IM_A_TEAPOT = 418,
		HTTP_419_AUTHENTICATION_TIMEOUT = 419,
		HTTP_422_UNPROCESSABLE_ENTITY = 422,
		HTTP_423_LOCKED = 423,
		HTTP_424_FAILED_DEPENDENCY = 424,
		HTTP_426_UPGRADE_REQUIRED = 426,
		HTTP_428_PRECONDITION_REQUIRED = 428,
		HTTP_429_TOO_MANY_REQUESTS = 429,
		HTTP_431_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
		HTTP_444_NO_RESPONSE = 444,

		HTTP_500_INTERNAL_SERVER_ERROR = 500,
		HTTP_501_NOT_IMPLEMENTED = 501,
		HTTP_502_BAD_GATEWAY = 502,
		HTTP_503_SERVICE_UNAVAILABLE = 503,
		HTTP_504_GATEWAY_TIMEOUT = 504,
		HTTP_505_HTTP_VERSION_NOT_SUPPORTED = 505,
		HTTP_506_VARIANT_ALSO_NEGOTIATES = 506,
		HTTP_507_INSUFFICIENT_STORAGE = 507,
		HTTP_508_LOOP_DETECTED = 508,
		HTTP_510_NOT_EXTENDED = 510,
		HTTP_511_NETWORK_AUTHENTICATION_REQUIRED = 511,
		HTTP_522_CONNECTION_TIMED_OUT = 522
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

	// The default reason string for a status code.
	// For unknown status codes, the string "-" is returned.
	static
	const char* default_reason(int code);

	// HTTP headers
	http_headers &headers();
	const http_headers &headers() const;
	void set_headers(const http_headers &headers);
	void set_headers(http_headers &&headers);

	std::vector<boost::asio::const_buffer> to_buffers() const;

protected:
	std::unique_ptr<http_response_data> m_data;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_HTTP_RESPONSE_HPP
