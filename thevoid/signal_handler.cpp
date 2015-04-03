#include "server.hpp"

#include <mutex>
#include <set>
#include <thread>
#include <atomic>

#include <signal.h>
#include <sys/prctl.h>

namespace ioremap { namespace thevoid { namespace signal_handler {

namespace {

// global mutex for all signal handling actions
std::mutex lock;

// library-wide monitoring servers
std::set<base_server*> servers;

// separate signal monitoring thread
std::thread monitoring_thread;
std::atomic<bool> running(false);

// received stop/reload signals
std::atomic<int> stop_request(-1);
std::atomic<int> reload_request(-1);

} // unnamed namespace

void add_server(base_server* server) {
	std::lock_guard<std::mutex> locker(lock);
	servers.insert(server);
}

void remove_server(base_server* server) {
	std::lock_guard<std::mutex> locker(lock);
	servers.erase(server);
}

void stop_sa_handler(int sig) {
	stop_request.store(sig);
}

void reload_sa_handler(int sig) {
	reload_request.store(sig);
}

bool register_stop(int signal_value) {
	std::lock_guard<std::mutex> locker(lock);

	struct sigaction sa;
	sa.sa_handler = stop_sa_handler;
	int err = sigaction(signal_value, &sa, NULL);

	return err == 0;
}

bool register_reload(int signal_value) {
	std::lock_guard<std::mutex> locker(lock);

	struct sigaction sa;
	sa.sa_handler = reload_sa_handler;
	int err = sigaction(signal_value, &sa, NULL);

	return err == 0;
}

static
void handle_stop(int signal_value) {
	std::lock_guard<std::mutex> locker(lock);

	for (auto it = servers.begin(); it != servers.end(); ++it) {
		BH_LOG((*it)->logger(), SWARM_LOG_INFO, "Handled signal [%d], stop server", signal_value);
		(*it)->stop();
	}
}

static
void handle_reload(int signal_value) {
	std::lock_guard<std::mutex> locker(lock);

	for (auto it = servers.begin(); it != servers.end(); ++it) {
		BH_LOG((*it)->logger(), SWARM_LOG_INFO, "Handled signal [%d], reload configuration", signal_value);
		try {
			(*it)->reload();
		} catch (std::exception &e) {
			std::fprintf(stderr, "Failed to reload configuration: %s", e.what());
		}
	}
}

static
void run() {
	char thread_name[16];
	memset(thread_name, 0, sizeof(thread_name));
	sprintf(thread_name, "void_signal");
	prctl(PR_SET_NAME, thread_name);

	while (running) {
		if (stop_request != -1) {
			int signal_value = stop_request;

			stop_request = -1;
			reload_request = -1;

			handle_stop(signal_value);
		}
		else if (reload_request != -1) {
			int signal_value = reload_request;

			reload_request = -1;

			handle_reload(signal_value);
		}
		else {
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
	}
}

void start() {
	if (running) {
		return;
	}

	running = true;
	monitoring_thread = std::thread(run);
}

void stop() {
	running = false;

	if (monitoring_thread.joinable()) {
		monitoring_thread.join();
	}
}

}}} // namespace ioremap::thevoid::signal_handler
