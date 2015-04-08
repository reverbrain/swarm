#include "signal_service_p.hpp"

#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include <memory>
#include <cstring>

namespace ioremap { namespace thevoid {

signal_service_state::signal_service_state()
	: read_descriptor(-1)
	, write_descriptor(-1)
{
	std::memset(registered, 0, sizeof(registered));
}

signal_service_state::~signal_service_state() {
	int saved_errno = errno;

	if (read_descriptor != -1) {
		::close(read_descriptor);
		read_descriptor = -1;
	}

	if (write_descriptor != -1) {
		::close(write_descriptor);
		write_descriptor = -1;
	}

	errno = saved_errno;
}

static
signal_service_state* get_signal_service_state() {
	static signal_service_state state;
	return &state;
}


static
void handle_signal_read(
		std::shared_ptr<boost::asio::posix::stream_descriptor> descriptor,
		std::shared_ptr<int> signal_buffer,
		const boost::system::error_code& error,
		std::size_t /* bytes_transferred */
	)
{
	// this callback will be called with "operation_aborted" error when
	// running io_service will be stopped
	if (error != boost::asio::error::operation_aborted) {
		signal_service_state* state = get_signal_service_state();
		std::lock_guard<std::mutex> lock(state->lock);

		// notify all signal services
		for (auto sig_service = state->services.begin(); sig_service != state->services.end(); ++sig_service) {
			auto* io_service = (*sig_service)->service;
			auto* server = (*sig_service)->server;
			int signal_number = *signal_buffer;
			auto handler = state->handlers[signal_number];

			io_service->post(std::bind(handler, signal_number, server));
		}

		// continue read from the descriptor
		auto buffer = boost::asio::buffer(signal_buffer.get(), sizeof(int));
		auto callback = boost::bind(handle_signal_read, descriptor, signal_buffer,
				boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);

		boost::asio::async_read(*descriptor, buffer, callback);
	}
}

static
void read_signal_descriptor(int read_descriptor, signal_service* service) {
	auto descriptor_ptr = std::make_shared<boost::asio::posix::stream_descriptor>(*service->service, read_descriptor);
	auto signal_buffer = std::make_shared<int>(0);
	auto buffer = boost::asio::buffer(signal_buffer.get(), sizeof(int));
	auto callback = boost::bind(handle_signal_read, descriptor_ptr, signal_buffer,
			boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);

	boost::asio::async_read(*descriptor_ptr, buffer, callback);
}

signal_service::signal_service(boost::asio::io_service* service, base_server* server)
	: service(service)
	, server(server)
{
	signal_service_state* state = get_signal_service_state();
	std::lock_guard<std::mutex> lock(state->lock);

	if (state->services.count(this) == 0) {
		state->services.insert(this);
		if (state->read_descriptor != -1) {
			read_signal_descriptor(state->read_descriptor, this);
		}
	}
}

signal_service::~signal_service() {
	signal_service_state* state = get_signal_service_state();
	std::lock_guard<std::mutex> lock(state->lock);

	state->services.erase(this);
}

static
int open_pipe(int &read_descriptor, int &write_descriptor) {
	int pipefd[2];
	if (::pipe2(pipefd, O_CLOEXEC | O_NONBLOCK) == 0) {
		read_descriptor = pipefd[0];
		write_descriptor = pipefd[1];
		return 0;
	}
	else {
		read_descriptor = -1;
		write_descriptor = -1;
		return -1;
	}
}

static
void handle_signal(int signal_number) {
	int saved_errno = errno;

	signal_service_state* state = get_signal_service_state();
	if (state->write_descriptor != -1) {
		::write(state->write_descriptor, &signal_number, sizeof(signal_number));
	}

	errno = saved_errno;
}

int register_signal_handler(int signal_number, signal_handler_type handler) {
	signal_service_state* state = get_signal_service_state();
	std::lock_guard<std::mutex> lock(state->lock);

	if (signal_number < 0 || signal_number >= NSIG) {
		// valid signal range is [0, NSIG)
		return -1;
	}

	if (state->registered[signal_number]) {
		// it's invalid to register signal twice
		return -1;
	}

	if (state->write_descriptor == -1) {
		// pipe is not open yet
		if (open_pipe(state->read_descriptor, state->write_descriptor) != 0) {
			return -1;
		}

		// if there're watching services -- request them to read the pipe
		for (auto sig_service = state->services.begin(); sig_service != state->services.end(); ++sig_service) {
			read_signal_descriptor(state->read_descriptor, *sig_service);
		}
	}

	struct sigaction sa;
	std::memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	sigfillset(&sa.sa_mask);

	if (::sigaction(signal_number, &sa, 0) == -1) {
		return -1;
	}

	state->registered[signal_number] = true;
	state->handlers[signal_number] = handler;

	return 0;
}

}} // namespace ioremap::thevoid
