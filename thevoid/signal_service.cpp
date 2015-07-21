#include "signal_service_p.hpp"

#include <boost/bind.hpp>

#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <sys/signalfd.h>
#include <sys/prctl.h>

#include <memory>
#include <cstring>

namespace ioremap { namespace thevoid {

signal_service_state::signal_service_state()
	: service(1)
	, signal_descriptor(-1)
{
}

signal_service_state::~signal_service_state() {
	int saved_errno = errno;

	if (signal_descriptor != -1) {
		::close(signal_descriptor);
		signal_descriptor = -1;
	}

	errno = saved_errno;
}

void signal_service_state::add_server(base_server* server) {
	std::lock_guard<std::mutex> locker(lock);

	servers.insert(server);
}

void signal_service_state::remove_server(base_server* server) {
	std::lock_guard<std::mutex> locker(lock);

	servers.erase(server);
}

signal_service_state* get_signal_service_state() {
	static signal_service_state state;
	return &state;
}


static
void handle_signal_read(
		std::shared_ptr<boost::asio::posix::stream_descriptor> descriptor,
		std::shared_ptr<signalfd_siginfo> siginfo_buffer,
		const boost::system::error_code& error,
		std::size_t bytes_transferred
	)
{
	// this callback will be called with "operation_aborted" error when
	// running io_service will be stopped
	if (error != boost::asio::error::operation_aborted) {
		signal_service_state* state = get_signal_service_state();
		std::lock_guard<std::mutex> lock(state->lock);

		if (bytes_transferred != sizeof(struct signalfd_siginfo)) {
			return;
		}

		int signal_number = siginfo_buffer->ssi_signo;
		auto handler = state->handlers[signal_number];

		for (auto server = state->servers.begin(); server != state->servers.end(); ++server) {
			handler(signal_number, *server);
		}

		// continue read from the descriptor
		auto buffer = boost::asio::buffer(siginfo_buffer.get(), sizeof(struct signalfd_siginfo));
		auto callback = boost::bind(handle_signal_read, descriptor, siginfo_buffer,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);

		boost::asio::async_read(*descriptor, buffer, callback);
	}
}

static
void read_signal_descriptor(int signal_descriptor, boost::asio::io_service& service) {
	if (signal_descriptor == -1) {
		return;
	}

	auto descriptor_ptr = std::make_shared<boost::asio::posix::stream_descriptor>(service, signal_descriptor);
	auto siginfo_buffer = std::make_shared<struct signalfd_siginfo>();
	auto buffer = boost::asio::buffer(siginfo_buffer.get(), sizeof(struct signalfd_siginfo));
	auto callback = boost::bind(handle_signal_read, descriptor_ptr, siginfo_buffer,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);

	boost::asio::async_read(*descriptor_ptr, buffer, callback);
}

int register_signal_handler(int signal_number, signal_handler_type handler) {
	signal_service_state* state = get_signal_service_state();
	std::lock_guard<std::mutex> lock(state->lock);

	if (signal_number < 0 || signal_number >= NSIG) {
		// valid signal range is [0, NSIG)
		return -1;
	}

	if (sigismember(&state->sigset, signal_number) == 1) {
		return -1;
	}

	sigaddset(&state->sigset, signal_number);

	if (state->signal_descriptor == -1) {
		sigset_t set;
		sigfillset(&set);
		if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
			return -1;
		}
	}

	state->signal_descriptor = signalfd(state->signal_descriptor, &state->sigset, SFD_NONBLOCK | SFD_CLOEXEC);

	state->handlers[signal_number] = handler;

	return 0;
}

static
void run_io_service(boost::asio::io_service& service) {
	prctl(PR_SET_NAME, "void_signal");

	boost::system::error_code ec;
	service.run(ec);
}

void run_signal_thread() {
	signal_service_state* state = get_signal_service_state();
	std::lock_guard<std::mutex> lock(state->lock);

	read_signal_descriptor(state->signal_descriptor, state->service);
	state->thread = std::thread(run_io_service, std::ref(state->service));
}

void stop_signal_thread() {
	signal_service_state* state = get_signal_service_state();

	state->service.stop();

	if (state->thread.joinable()) {
		state->thread.join();
	}
}

}} // namespace ioremap::thevoid
