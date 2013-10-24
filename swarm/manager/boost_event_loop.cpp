#include "boost_event_loop.hpp"

#include <boost/bind.hpp>
#include <memory>

namespace ioremap {
namespace swarm {

boost_event_loop::boost_event_loop(boost::asio::io_service &service) :
	m_service(service), m_timer(service)
{
}

struct boost_socket_info
{
	typedef std::shared_ptr<boost_socket_info> ptr;

	boost_socket_info(boost::asio::io_service &service, int fd, event_loop::poll_option what) :
		socket(service), what(what)
	{
		socket.assign(boost::asio::local::stream_protocol(), fd);
	}

	boost::asio::local::stream_protocol::socket socket;
	event_loop::poll_option what;
};

int boost_event_loop::socket_request(int fd, poll_option what, void *data)
{
	auto *info = reinterpret_cast<boost_socket_info *>(data);

	if (what == poll_remove) {
		logger().log(LOG_ERROR, "remove socket: %p, fd: %d", info, fd);
		delete info;
		return 0;
	}

	if (!info) {
		info = new boost_socket_info(m_service, fd, what);
		logger().log(LOG_ERROR, "create socket: %p, fd: %d", info, fd);
		listener()->set_socket_data(fd, info);
	}
	logger().log(LOG_ERROR, "poll socket: %p, fd: %d, what: %d", info, fd, what);

	if (what == poll_in) {
		logger().log(LOG_ERROR, "poll in socket: %p, fd: %d", info, fd);
		info->socket.async_read_some(boost::asio::null_buffers(),
			boost::bind(&boost_event_loop::on_event, this, fd, info, event_listener::socket_read));
	}
	if (what == poll_out) {
		logger().log(LOG_ERROR, "poll out socket: %p, fd: %d", info, fd);
		info->socket.async_write_some(boost::asio::null_buffers(),
			boost::bind(&boost_event_loop::on_event, this, fd, info, event_listener::socket_write));
	}

	return 0;
}

int boost_event_loop::timer_request(long timeout_ms)
{
	logger().log(LOG_ERROR, "timer: %ld", timeout_ms);
	m_timer.cancel();

	if (timeout_ms == 0) {
		m_service.post(std::bind(&event_listener::on_timer, listener()));
	} else {
		m_timer.expires_from_now(boost::posix_time::millisec(timeout_ms));
		m_timer.async_wait(std::bind(&event_listener::on_timer, listener()));
	}

	return 0;
}

void boost_event_loop::post(const std::function<void ()> &func)
{
	logger().log(LOG_ERROR, "post");
	m_service.post(func);
}

void boost_event_loop::on_event(int fd, boost_socket_info *info, int what)
{
	logger().log(LOG_ERROR, "on_event socket: %p, fd: %d, info->what: %d, what: %d", info, fd, info->what, what);
	if (what == event_listener::socket_read && (info->what & poll_in)) {
		logger().log(LOG_ERROR, "repoll in socket: %p, fd: %d", info, fd);
		info->socket.async_read_some(boost::asio::null_buffers(),
			boost::bind(&boost_event_loop::on_event, this, fd, info, event_listener::socket_read));
	}
	if (what == event_listener::socket_write && (info->what & poll_out)) {
		logger().log(LOG_ERROR, "repoll out socket: %p, fd: %d", info, fd);
		info->socket.async_write_some(boost::asio::null_buffers(),
			boost::bind(&boost_event_loop::on_event, this, fd, info, event_listener::socket_write));
	}

	logger().log(LOG_ERROR, "call on_socket_event");

	listener()->on_socket_event(fd, what);
}

} // namespace swarm
} // namespace ioremap
