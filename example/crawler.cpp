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

#include <iostream>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <set>
#include <list>
#include <cstring>
#include <unistd.h>
#include <algorithm>
#include <functional>

#if __GNUC__ == 4 && __GNUC_MINOR__ < 5
#  include <cstdatomic>
#else
#  include <atomic>
#endif

#include <fcntl.h>
#include <sys/prctl.h>

#include <swarm/networkmanager.h>
#include <swarm/url_finder.h>
#include <swarm/network_url.h>

struct queue_element
{
	ioremap::swarm::http_request request;
	std::string url;
	std::string data;
	int depth;
};

struct crawler_scope
{
	int need_to_load;
	int active_threads;
	int nm_limit;
	bool done;
	std::atomic_long in_progress;
	std::atomic_long counter;

	std::string base_host;
	std::string base_directory;
	std::list<queue_element> files;
	std::set<std::string> used;
	std::vector<ioremap::swarm::network_manager*> managers;
	std::vector<ev::async*> asyncs;
	std::condition_variable condition;
	std::condition_variable fs_condition;
	std::mutex mutex;
	std::mutex fs_mutex;

	void check_end(int current_in_progress) {
		if (current_in_progress == 0) {
			force_end();
		}
	}

	void force_end() {
		std::for_each(asyncs.begin(), asyncs.end(), std::bind(&ev::async::send, std::placeholders::_1));
		std::unique_lock<std::mutex> lock(fs_mutex);
		done = true;
		fs_condition.notify_all();
	}
};

struct result_handler
{
	crawler_scope &scope;
	int depth;

	void operator() (const ioremap::swarm::http_response &reply) {
		++scope.counter;

		if (reply.code() == 200 && !reply.error()) {
			++scope.in_progress;
			queue_element element = { reply.request(), reply.url(), reply.data(), depth - 1 };
			std::unique_lock<std::mutex> lock(scope.fs_mutex);
			scope.files.push_back(element);
			scope.fs_condition.notify_all();
		}

		if (reply.error()) {
			std::cerr << "Error at \"" << reply.request().url() << "\": " << strerror(-reply.error()) << ": " << reply.error() << std::endl;
		}

		scope.check_end(--scope.in_progress);
	}
};

struct ev_loop_stopper
{
	ev::loop_ref &loop;

	void operator() (ev::async &, int) {
		loop.unloop();
	}
};

struct network_manager_thread
{
	crawler_scope &scope;

	void operator() () {
		prctl(PR_SET_NAME, "swarm-curl");
		ev::dynamic_loop loop;

		ev::async async(loop);
		ev_loop_stopper stopper = { loop };
		async.set<ev_loop_stopper>(&stopper);
		async.start();

		ioremap::swarm::network_manager manager(loop);
		manager.set_limit(scope.nm_limit);
		{
			std::lock_guard<std::mutex> lock(scope.mutex);
			++scope.active_threads;
			scope.managers.push_back(&manager);
			scope.asyncs.push_back(&async);
			scope.condition.notify_all();
		}
		loop.loop();
	}
};

struct fs_thread
{
	crawler_scope &scope;

	struct in_progress_guard
	{
		crawler_scope &scope;

		~in_progress_guard() { scope.check_end(--scope.in_progress); }
	};

	void operator() ()
	{
		prctl(PR_SET_NAME, "swarm-fs");
		{
			std::unique_lock<std::mutex> lock(scope.mutex);
			++scope.active_threads;
			scope.condition.notify_all();
		}
		for (;;) {
			queue_element element;
			{
				std::unique_lock<std::mutex> fs_lock(scope.fs_mutex);
				while (scope.files.empty() && !scope.done)
					scope.fs_condition.wait(fs_lock);
				if (!scope.files.empty()) {
					element = scope.files.front();
					scope.files.erase(scope.files.begin());
				} else if (scope.done) {
					break;
				}
			}
			in_progress_guard guard = { scope };

			ioremap::swarm::network_url base_url;
			ioremap::swarm::url_finder finder(element.data);

			if (!base_url.set_base(element.url))
				continue;

			element.url = base_url.normalized();
			if (element.url.empty())
				continue;

			result_handler handler = { scope, element.depth };
			if (element.depth >= 0) {
				for (auto it = finder.urls().begin(); it != finder.urls().end(); ++it) {
					std::string url = *it;

					if (url.compare(0, 7, "mailto:") == 0)
						continue;

					std::string host;
					element.request.set_url(base_url.relative(url, &host));
					if (element.request.url().empty() || host.empty())
						continue;

					if (!scope.base_host.empty()) {
						if (host.size() < scope.base_host.size())
							continue;

						if (host.size() > scope.base_host.size()) {
							if (host.find(scope.base_host) != host.size() - scope.base_host.size())
								continue;
							if (host[host.size() - scope.base_host.size() - 1] != '.')
								continue;
						} else if (host != scope.base_host) {
							continue;
						}
					}

					bool inserted = false;
					{
						std::lock_guard<std::mutex> lock(scope.mutex);
						if (scope.need_to_load > 0) {
							inserted = scope.used.insert(element.request.url()).second;
							if (inserted)
								--scope.need_to_load;
						}
					}
					if (inserted) {
						++scope.in_progress;
						scope.managers[rand() % scope.managers.size()]->get(handler, element.request);
					}
				}
			}

			std::string filepath = scope.base_directory + "/" + element.url;
			size_t index = 0;
			while ((index = filepath.find("//", index)) != std::string::npos) {
				filepath.erase(index, 1);
			}
			if (filepath[filepath.size() - 1] == '/')
				filepath.resize(filepath.size() - 1);
			filepath += "~file-tag";


			index = 0;
			int err = 0;
			while ((index = filepath.find('/', index + 1)) != std::string::npos) {
				std::string dir = filepath.substr(0, index);
				err = mkdir(dir.c_str(), 0755);
				if (err < 0) {
					if (errno != EEXIST) {
						err = errno;
						std::cerr << "Can not create directory: \"" << scope.base_directory << "\": " << strerror(err) << std::endl;
						break;
					} else {
						err = 0;
					}
				}
			}
			if (err != 0)
				continue;

			int oflags = O_RDWR | O_CREAT | O_LARGEFILE | O_CLOEXEC | O_TRUNC;
			int fd = open(filepath.c_str(), oflags, 0644);
			if (fd < 0) {
				err = errno;
				std::cerr << "Can not create file: \"" << filepath << "\": " << strerror(err) << std::endl;
				continue;
			}
			ssize_t written = pwrite(fd, element.data.c_str(), element.data.size(), 0);
			close(fd);
			if (written != (ssize_t)element.data.size()) {
				err = errno;
				std::cerr << "Can not write data to : \"" << filepath << "\": " << strerror(err) << std::endl;
				continue;
			}
		}
	}
};

struct rps_counter
{
	std::atomic_long &counter;
	long previous_counter;

	void operator() (ev::timer &, int) {
		long new_counter = counter;
		long delta = new_counter - previous_counter;
		previous_counter = new_counter;
		std::cout << "rps: " << delta << std::endl;
	}
};

struct sig_handler
{
	crawler_scope &scope;

	void operator() (ev::sig &, int) {
		scope.force_end();
	}
};

int main(int argc, char **argv)
{
	if (argc < 5 || argc > 10) {
		std::cerr << "usage: " << argv[0] << " url max_depth max_count base_directory [curl_jobs [fs_jobs [connections_per_curl follow_other_hosts]]]]" << std::endl;
		return 1;
	}

	crawler_scope scope;

	scope.nm_limit = 25;
	scope.active_threads = 0;
	scope.done = false;
	scope.in_progress = 0;
	scope.counter = 0;

	std::string url = argv[1];
	int max_depth = atoi(argv[2]);
	scope.need_to_load = atoi(argv[3]);
	if (scope.need_to_load == -1) {
		scope.need_to_load = std::numeric_limits<int>::max();
	}
	if (max_depth == -1) {
		max_depth = std::numeric_limits<int>::max();
	}
	scope.base_directory = argv[4];
	size_t thread_count = std::thread::hardware_concurrency();
	size_t fs_thread_count = std::thread::hardware_concurrency();
	if (argc > 5)
		thread_count = std::max(1, atoi(argv[5]));
	if (argc > 6)
		fs_thread_count = std::max(1, atoi(argv[6]));
	if (argc > 7)
		scope.nm_limit = std::max(1, atoi(argv[7]));


	ioremap::swarm::network_url url_parser;
	url_parser.set_base(url);
	// normalize url
	std::string normalized_url = url_parser.normalized();
	if (normalized_url.empty()) {
		std::cerr << "Url is invalid: \"" << url << "\"" << std::endl;
		return 2;
	}
	url = normalized_url;

	if (argc < 9 || !atoi(argv[8]))
		scope.base_host = url_parser.host();

	int err = mkdir(scope.base_directory.c_str(), 0755);
	if (err < 0) {
		if (errno != EEXIST) {
			err = errno;
			std::cerr << "Can not create directory: \"" << scope.base_directory << "\": " << strerror(err) << std::endl;
			return err;
		}
	}

	ev::default_loop loop;

	sig_handler shandler = { scope };

	std::list<ev::sig> sigs;

	int signal_ids[] = { SIGINT, SIGTERM };

	for (size_t i = 0; i < sizeof(signal_ids) / sizeof(signal_ids[0]); ++i) {
		sigs.emplace_back(loop);
		ev::sig &sig_watcher = sigs.back();
		sig_watcher.set(signal_ids[i]);
		sig_watcher.set<sig_handler>(&shandler);
		sig_watcher.start();
	}

	rps_counter counter = { scope.counter, scope.counter };
	network_manager_thread thread_handler = { scope };
	fs_thread fs_thread_handler = { scope };
	std::vector<std::thread> threads;

	for (size_t i = 0; i < thread_count; ++i) {
		threads.emplace_back(thread_handler);
	}

	for (size_t i = 0; i < fs_thread_count; ++i) {
		threads.emplace_back(fs_thread_handler);
	}

	{
		std::unique_lock<std::mutex> lock(scope.mutex);
		while (thread_count != scope.asyncs.size())
			scope.condition.wait(lock);
	}

	ev::timer timer(loop);
	timer.set<rps_counter>(&counter);
	timer.start(1.0f, 1.0f);

	ev::async async(loop);
	ev_loop_stopper stopper = { loop };
	async.set<ev_loop_stopper>(&stopper);
	async.start();
	scope.asyncs.push_back(&async);

	ioremap::swarm::http_request request;
	request.set_follow_location(true);
	result_handler handler = { scope, max_depth };
	request.set_url(url);
	--scope.need_to_load;
	++scope.in_progress;
	scope.managers[rand() % scope.managers.size()]->get(handler, request);

	loop.loop();

	// make sure that all threads finished their work
	std::for_each(threads.begin(), threads.end(),
		std::bind(&std::thread::join, std::placeholders::_1));

	counter(timer, 0);

	return 0;
}

