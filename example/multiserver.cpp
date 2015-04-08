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

#include <unistd.h>
#include <signal.h>

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

		return true;
	}

	struct on_ping : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const thevoid::http_request &req, const boost::asio::const_buffer &buffer) {
			(void) buffer;
			(void) req;

			this->send_reply(thevoid::http_response::ok);
		}
	};

};

void run_server(char* config) {
	char *argv[] = {"multiserver", "--config", config};
	int argc = sizeof(argv) / sizeof(char*);

	auto server = ioremap::thevoid::create_server<http_server>();
	int err = server->run(argc, argv);

	if (err != 0) {
		std::cerr << "Server exited with error code " << err << std::endl;
	}
}

const char msg[] = "SIGALRM signal handled manually\n";

void sigalrm_signal_handler(int /* signal_number */) {
	// write message to stderr
	::write(STDERR_FILENO, msg, sizeof(msg) - 1);
}

int main(int argc, char **argv)
{
	if (argc == 1) {
		std::cerr << "Usage: <program> <server1 config> <server2 config> ..." << std::endl;
		return -1;
	}

	std::vector<std::thread> servers;
	for (int i = 1; i < argc; ++i) {
		servers.push_back(std::thread(run_server, argv[i]));
	}

	// register custom signal handlers (even after creating servers)
	// one can comment out the following piece of code to disable thevoid's signal handling
	ioremap::thevoid::register_signal_handler(SIGINT, ioremap::thevoid::handle_stop_signal);
	ioremap::thevoid::register_signal_handler(SIGTERM, ioremap::thevoid::handle_stop_signal);
	ioremap::thevoid::register_signal_handler(SIGHUP, ioremap::thevoid::handle_reload_signal);
	ioremap::thevoid::register_signal_handler(SIGUSR1, ioremap::thevoid::handle_ignore_signal);
	ioremap::thevoid::register_signal_handler(SIGUSR2, ioremap::thevoid::handle_ignore_signal);

	// one can register their own signal handlers
	{
		struct sigaction sa;
		std::memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sigalrm_signal_handler;
		sigfillset(&sa.sa_mask);

		::sigaction(SIGALRM, &sa, 0);
	}

	for (size_t i = 0; i < servers.size(); ++i) {
		servers[i].join();
	}
}
