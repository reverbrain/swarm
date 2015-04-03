#include "signal_handler_p.hpp"

#include <cstring>
#include <functional>

#include <signal.h>

#include "server_p.hpp"

namespace ioremap { namespace thevoid {

static
void stop_sa_handler(int sig) {
	signal_handler::stop_request.store(sig);
}

static
void reload_sa_handler(int sig) {
	signal_handler::reload_request.store(sig);
	signal_handler::reload_rev++;
}

bool register_stop_signal(int signal_value) {
	struct sigaction sa;
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = stop_sa_handler;
	sigfillset(&sa.sa_mask);
	int err = sigaction(signal_value, &sa, NULL);

	return err == 0;
}

bool register_reload_signal(int signal_value) {
	struct sigaction sa;
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = reload_sa_handler;
	sigfillset(&sa.sa_mask);
	int err = sigaction(signal_value, &sa, NULL);

	return err == 0;
}

std::atomic<int> signal_handler::stop_request(-1);

std::atomic<int> signal_handler::reload_request(-1);
std::atomic<size_t> signal_handler::reload_rev(0);

signal_handler::signal_handler(
		base_server* server,
		const boost::posix_time::time_duration &timeout
	)
	: m_server(server)
	, m_timeout(timeout)
	, m_reload_signal_rev(0)
{
}

void signal_handler::run(boost::asio::io_service* io_service) {
	// init server's reload signal revision number with global one
	m_reload_signal_rev = reload_rev;

	m_timer = timer_ptr(new timer(*io_service));

	m_timer->expires_from_now(m_timeout);
	m_timer->async_wait(std::bind(&signal_handler::handle_timer, this, std::placeholders::_1));
}

void signal_handler::handle_timer(const boost::system::error_code &error)
{
	if (error != boost::asio::error::operation_aborted) {
		// check stop_request first
		if (stop_request != -1) {
			int signal_value = stop_request;
			BH_LOG(m_server->logger(), SWARM_LOG_INFO, "Handled signal [%d], stop server", signal_value);
			m_server->stop();
			return;
		}

		// check reload_request
		if (reload_request != -1 && m_reload_signal_rev != reload_rev) {
			m_reload_signal_rev = reload_rev;

			int signal_value = reload_request;
			BH_LOG(m_server->logger(), SWARM_LOG_INFO, "Handled signal [%d], reload configuration", signal_value);
			try {
				m_server->reload();
			} catch (std::exception &e) {
				std::fprintf(stderr, "Failed to reload configuration: %s", e.what());
			}
		}

		// wait for next timeout
		m_timer->expires_from_now(m_timeout);
		m_timer->async_wait(std::bind(&signal_handler::handle_timer, this, std::placeholders::_1));
	}
}

}} // namespace ioremap::thevoid
