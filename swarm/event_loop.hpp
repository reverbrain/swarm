#ifndef IOREMAP_SWARM_EVENT_LOOP_H
#define IOREMAP_SWARM_EVENT_LOOP_H

#include <functional>
#include "logger.hpp"

namespace ioremap {
namespace swarm {

class event_listener
{
public:
	enum socket_action {
		socket_read = 0x01,
		socket_write = 0x02,
		socket_all = socket_read | socket_write
	};

	virtual ~event_listener();

	virtual void set_socket_data(int socket, void *data) = 0;
	virtual void on_timer() = 0;
	virtual void on_socket_event(int socket, int action) = 0;
};

class event_loop
{
public:
	enum poll_option {
		poll_in = 0x01,
		poll_out = 0x02,
		poll_all = poll_in | poll_out,
		poll_remove = 0x04
	};

	event_loop();
	virtual ~event_loop();

	void set_listener(event_listener *listener);
	event_listener *listener() const;

	void set_logger(const swarm::logger &logger);
	swarm::logger logger() const;

	virtual int socket_request(int socket, poll_option what, void *data) = 0;
	virtual int timer_request(long timeout_ms) = 0;
	virtual void post(const std::function<void ()> &func) = 0;

private:
	swarm::logger m_logger;
	event_listener *m_listener;
};

}} // namespace ioremap::swarm

#endif // IOREMAP_SWARM_EVENT_LOOP_H
