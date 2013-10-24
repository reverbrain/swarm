#ifndef IOREMAP_SWARM_BOOST_EVENT_LOOP_H
#define IOREMAP_SWARM_BOOST_EVENT_LOOP_H

#include "event_loop.hpp"

#include <boost/asio.hpp>

namespace ioremap {
namespace swarm {

struct boost_socket_info;

class boost_event_loop : public event_loop
{
public:
	boost_event_loop(boost::asio::io_service &service);

	int socket_request(int socket, poll_option what, void *data);
	int timer_request(long timeout_ms);
	void post(const std::function<void ()> &func);

private:
	void on_event(int fd, boost_socket_info *info, int what);

	boost::asio::io_service &m_service;
	boost::asio::deadline_timer m_timer;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_BOOST_EVENT_LOOP_H
