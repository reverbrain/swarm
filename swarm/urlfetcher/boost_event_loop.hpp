#ifndef IOREMAP_SWARM_BOOST_EVENT_LOOP_H
#define IOREMAP_SWARM_BOOST_EVENT_LOOP_H

#include "event_loop.hpp"

#include <boost/asio.hpp>
#include <memory>
#include <unordered_map>

namespace ioremap {
namespace swarm {

struct boost_socket_info;

class boost_event_loop : public event_loop
{
public:
	boost_event_loop(boost::asio::io_service &service);

	int open_socket(int domain, int type, int protocol);
	int close_socket(int fd);
	int socket_request(int socket, poll_option what, void *data);
	int timer_request(long timeout_ms);
	void post(const std::function<void ()> &func);

private:
	void on_event(int fd, const std::weak_ptr<boost_socket_info> &info, int what, const boost::system::error_code &error);

	boost::asio::io_service &m_service;
	boost::asio::deadline_timer m_timer;
	std::unordered_map<int, std::shared_ptr<boost_socket_info>> m_sockets;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_BOOST_EVENT_LOOP_H
