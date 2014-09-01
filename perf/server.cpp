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

template <typename T>
struct on_upload : public thevoid::simple_request_stream<T>, public std::enable_shared_from_this<on_upload<T>>
{
	virtual void on_request(const thevoid::http_request &req, const boost::asio::const_buffer &buffer) {
		const swarm::url_query &query_list = req.url().query();

		(void) buffer;

		auto name = query_list.item_value("name");

		if (!name) {
			this->send_reply(ioremap::thevoid::http_response::bad_request);
			return;
		}

		std::string data = "POST reply\n";
		thevoid::http_response reply;
		reply.set_code(thevoid::http_response::ok);
		reply.headers().set_content_length(data.size());
		reply.headers().set_content_type("text/plain");
		this->send_reply(std::move(reply), std::move(data));
	}
};

template <typename T>
struct on_get : public thevoid::simple_request_stream<T>, public std::enable_shared_from_this<on_get<T>>
{
	virtual void on_request(const thevoid::http_request &req, const boost::asio::const_buffer &buffer) {
		using namespace std::placeholders;

		(void) buffer;
		(void) req;

		std::string data = "GET reply\n";
		thevoid::http_response reply;
		reply.set_code(thevoid::http_response::ok);
		reply.headers().set_content_length(data.size());
		reply.headers().set_content_type("text/plain");

		this->send_reply(std::move(reply), std::move(data));
	}
};


class http_server : public thevoid::server<http_server>
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		(void) config;

//		on<on_get<http_server>>(
//			options::exact_match("/get"),
//			options::methods("GET")
//		);
//		on<on_upload<http_server>>(
//			options::exact_match("/upload"),
//			options::methods("POST")
//		);
	
		return true;
	}

private:
};

int main(int argc, char **argv)
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}
