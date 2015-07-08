/*
 * Copyright 2015+ Danil Osherov <shindo@yandex-team.ru>
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

#include "server.hpp"
#include "handlers_factory.hpp"

#include "thevoid/server.hpp"


class server : public ioremap::thevoid::server<server> {
public:
	void register_handler(const rapidjson::Value& config) {
		base_server::options opts;

		const auto& exact_match = config["exact_match"];
		if (!exact_match.IsNull()) {
			opts.set_exact_match(exact_match.GetString());
		}

		const auto& prefix_match = config["prefix_match"];
		if (!prefix_match.IsNull()) {
			opts.set_prefix_match(prefix_match.GetString());
		}

		const auto& regex_match = config["regex_match"];
		if (!regex_match.IsNull()) {
			opts.set_regex_match(regex_match.GetString());
		}

		const auto& methods = config["methods"];
		if (!methods.IsNull()) {
			std::vector<std::string> v;
			for (size_t i = 0, size = methods.Size(); i < size; ++i) {
				v.push_back(methods[i].GetString());
			}

			opts.set_methods(v);
		}

		const auto& headers = config["headers"];
		if (!headers.IsNull()) {
			for (auto iter = headers.MemberBegin(), end = headers.MemberEnd();
					iter != end; ++iter) {
				opts.set_header(iter->name.GetString(), iter->value.GetString());
			}
		}

		std::string handler = config["handler"].GetString();
		base_server::on(std::move(opts), handlers::factory.at(handler));
	}

	virtual bool initialize(const rapidjson::Value &config) {
		auto& handlers = config["handlers"];
		for (auto iter = handlers.Begin(), end = handlers.End(); iter != end; ++iter) {
			register_handler(*iter);
		}

		return true;
	}
};


std::shared_ptr<server> server_ptr;

__attribute__((constructor(1000)))
static
void init_server() {
	server_ptr = std::shared_ptr<server>(new server());
}


int main(int argc, char **argv)
{
	ioremap::thevoid::register_signal_handler(SIGINT, ioremap::thevoid::handle_stop_signal);
	ioremap::thevoid::register_signal_handler(SIGTERM, ioremap::thevoid::handle_stop_signal);

	ioremap::thevoid::run_signal_thread();

	int err = server_ptr->run(argc, argv);

	ioremap::thevoid::stop_signal_thread();

	return err;
}
