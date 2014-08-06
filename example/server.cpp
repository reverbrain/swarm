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

#include <swarm/http_request.hpp>
#include <swarm/urlfetcher/url_fetcher.hpp>
#include <thevoid/server.hpp>
#include <thevoid/stream.hpp>

using namespace ioremap;

class http_server : public thevoid::server<http_server>
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		(void) config;

		on<on_ping>(
			options::exact_match("/ping"),
			options::methods("GET")
		);
		on<on_timeout>(
			options::exact_match("/timeout"),
			options::methods("GET")
		);
		on<on_get>(
			options::exact_match("/get"),
			options::methods("GET")
		);
		on<on_echo>(
			options::exact_match("/echo"),
			options::methods("GET")
		);
		on<on_ping>(
			options::exact_match("/header-check"),
			options::methods("GET"),
			options::header("X-CHECK", "SecretKey")
		);
	
		return true;
	}

	struct on_ping : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) {
			(void) buffer;
			(void) req;

			this->send_reply(swarm::http_response::ok);
		}
	};

	struct on_timeout : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) {
			(void) buffer;
			(void) req;
			if (auto timeout = req.url().query().item_value("timeout")) {
				BH_LOG(logger(), SWARM_LOG_INFO, "timeout: %s", timeout->c_str());
				usleep(atoi(timeout->c_str()) * 1000);
			}

			this->send_reply(swarm::http_response::ok);
		}
	};

	struct on_get : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) {
			(void) buffer;

			std::string data;
			if (auto datap = req.url().query().item_value("data"))
				data = *datap;

			int timeout_ms = 10 + (rand() % 10);
			usleep(timeout_ms * 1000);

			swarm::http_response reply;
			reply.set_code(swarm::http_response::ok);
			reply.headers().set_content_length(data.size());

			this->send_reply(std::move(reply), std::move(data));
		}
	};

	struct on_echo : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) {
			auto data = boost::asio::buffer_cast<const char*>(buffer);
			auto size = boost::asio::buffer_size(buffer);

			swarm::http_response reply;
			reply.set_code(swarm::http_response::ok);
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
