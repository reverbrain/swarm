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
		on<on_chunked>(
			options::exact_match("/chunked"),
			options::methods("POST")
		);
		on<on_ping>(
			options::exact_match("/header-check"),
			options::methods("GET"),
			options::header("X-CHECK", "SecretKey")
		);
	
		return true;
	}

	struct on_ping : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const thevoid::http_request &req, const boost::asio::const_buffer &buffer) {
			(void) buffer;
			(void) req;

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

	struct on_chunked : public thevoid::buffered_request_stream<http_server> {
		virtual void on_request(const thevoid::http_request &req) {
			(void) req;

			this->try_next_chunk();
		}

		virtual void on_chunk(const boost::asio::const_buffer &buffer, unsigned int flags) {
			auto data = boost::asio::buffer_cast<const char*>(buffer);
			size_t size = boost::asio::buffer_size(buffer);

			m_data.insert(m_data.end(), data, data + size);
			BH_LOG(this->server()->logger(), SWARM_LOG_DEBUG, "received chunk: size: %ld, total_size: %ld, flags: 0x%x", size, m_data.size(), flags);

			if (flags & thevoid::buffered_request_stream<http_server>::last_chunk) {
				thevoid::http_response reply;
				reply.set_code(thevoid::http_response::ok);
				reply.headers().set_content_length(m_data.size());
				reply.headers().set("X-Total-Size", std::to_string(m_data.size()));

				this->send_reply(std::move(reply), std::move(m_data));
			} else {
				this->try_next_chunk();
			}
		}

		virtual void on_error(const boost::system::error_code &err) {
			BH_LOG(this->server()->logger(), SWARM_LOG_ERROR, "connection error: %s [%d]", err.message(), err.value());
		}

		std::vector<char> m_data;
	};
};

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}
