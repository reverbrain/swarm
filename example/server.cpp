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

#include <thevoid/server.hpp>
#include <thevoid/stream.hpp>
#include <thevoid/options.hpp>

using namespace ioremap;

template <typename T>
void register_handlers(T)
{
}

template <typename T>
std::shared_ptr<thevoid::base_stream_factory> create_handler()
{
	return std::shared_ptr<thevoid::base_stream_factory>();
}

namespace ioremap { namespace thevoid { namespace options {

template <typename... Args>
void select(Args... args)
{
	typedef decltype(std::make_tuple(args...)) args_tuple;
	static_assert(std::tuple_size<args_tuple>::value > 0, "There must be at least one argument");
	static_assert(std::is_same<
		typename std::tuple_element<std::tuple_size<args_tuple>::value - 1, args_tuple>::type,
		http_response::status_type
		>::value,
		"Last element must be a http_response::status");
}

} } }

class http_server : public thevoid::server<http_server>
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		(void) config;

		using namespace ioremap::thevoid;

		options::select(thevoid::http_response::method_not_allowed);

		options::select(
			options::method("GET"),
			create_handler<on_ping>(),
			thevoid::http_response::method_not_allowed
		);

//		register_handlers(options::select(
//			options::exact_match("/ping") = options::select(
//				options::method("GET") = create_handler<on_ping>(),
//				thevoid::http_response::method_not_allowed
//			),
//			options::exact_match("/timeout") = options::select(
//				options::method("GET") = create_handler<on_timeout>(),
//				thevoid::http_response::method_not_allowed
//			),
//			options::exact_match("/get") = options::select(
//				options::method("GET") = create_handler<on_get>(),
//				thevoid::http_response::method_not_allowed
//			),
//			options::exact_match("/echo") = options::select(
//				options::method("GET") = create_handler<on_echo>(),
//				thevoid::http_response::method_not_allowed
//			),
//			options::exact_match("/header-check") = options::select(
//				options::method("GET") = options::select(
//					options::header("X-CHECK", "SecretKey") = create_handler<on_ping>(),
//					thevoid::http_response::not_acceptable
//				),
//				thevoid::http_response::method_not_allowed
//			),
//			thevoid::http_response::not_found
//		));

//		on<on_ping>(
//			options::exact_match("/ping"),
//			options::methods("GET")
//		);
//		on<on_timeout>(
//			options::exact_match("/timeout"),
//			options::methods("GET")
//		);
//		on<on_get>(
//			options::exact_match("/get"),
//			options::methods("GET")
//		);
//		on<on_echo>(
//			options::exact_match("/echo"),
//			options::methods("GET")
//		);
//		on<on_ping>(
//			options::exact_match("/header-check"),
//			options::methods("GET"),
//			options::header("X-CHECK", "SecretKey")
//		);
	
		return true;
	}

	struct on_ping : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const thevoid::http_request &req, const boost::asio::const_buffer &buffer) {
			(void) buffer;
			(void) req;

			thevoid::detail::options::option_base<thevoid::detail::options::option_exact_match> first("/ping");
			thevoid::detail::options::option_base<thevoid::detail::options::option_true> second;

			std::function<bool (const thevoid::http_request &request)> match = std::move(first) && std::move(second);
			std::cout << "match: " << match(req) << std::endl;

			this->send_reply(thevoid::http_response::ok);
		}
	};

	struct on_timeout : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const thevoid::http_request &req, const boost::asio::const_buffer &buffer) {
			(void) buffer;
			(void) req;
			if (auto timeout = req.url().query().item_value("timeout")) {
				BH_LOG(logger(), SWARM_LOG_INFO, "timeout: %s", timeout->c_str());
				usleep(atoi(timeout->c_str()) * 1000);
			}

			this->send_reply(thevoid::http_response::ok);
		}
	};

	struct on_get : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const thevoid::http_request &req, const boost::asio::const_buffer &buffer) {
			(void) buffer;

			std::string data;
			if (auto datap = req.url().query().item_value("data"))
				data = *datap;

			int timeout_ms = 10 + (rand() % 10);
			usleep(timeout_ms * 1000);

			thevoid::http_response reply;
			reply.set_code(thevoid::http_response::ok);
			reply.headers().set_content_length(data.size());

			this->send_reply(std::move(reply), std::move(data));
		}
	};

	struct on_echo : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const thevoid::http_request &req, const boost::asio::const_buffer &buffer) {
			auto data = boost::asio::buffer_cast<const char*>(buffer);
			auto size = boost::asio::buffer_size(buffer);

			thevoid::http_response reply;
			reply.set_code(thevoid::http_response::ok);
			reply.set_headers(req.headers());
			reply.headers().set_content_length(size);

			this->send_reply(std::move(reply), std::string(data, size));
		}
	};
};

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}
