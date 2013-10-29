#include "event_loop.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ioremap {
namespace swarm {


event_listener::~event_listener()
{
}

event_loop::event_loop() : m_listener(NULL)
{
}

event_loop::~event_loop()
{
}

void event_loop::set_listener(event_listener *listener)
{
	m_listener = listener;
}

event_listener *event_loop::listener() const
{
	return m_listener;
}

void event_loop::set_logger(const swarm::logger &logger)
{
	m_logger = logger;
}

logger event_loop::logger() const
{
	return m_logger;
}

int event_loop::open_socket(int domain, int type, int protocol)
{
	return ::socket(domain, type, protocol);
}

int event_loop::close_socket(int fd)
{
	return ::close(fd);
}

}} // namespace ioremap::swarm
