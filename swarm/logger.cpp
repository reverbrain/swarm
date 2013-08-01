#include "logger.h"
#include <fstream>
#include <ctime>
#include <unistd.h>
#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <sys/time.h>

/*
 * Supported in Linux only so far
 */
#ifdef HAVE_SENDFILE4_SUPPORT
#include <sys/syscall.h>
static long get_thread_id()
{
	return syscall(SYS_gettid);
}
#else
#include <pthread.h>
static long get_thread_id()
{
	return pthread_self();
}
#endif

namespace ioremap {
namespace swarm {

class file_logger_interface : public logger_interface
{
public:
	file_logger_interface(const char *file) {
		m_stream.open(file, std::ios_base::app);
		if (!m_stream) {
			std::string message = "Can not open file: \"";
			message += file;
			message += "\"";
			throw std::ios_base::failure(message);
		}
		m_stream.exceptions(std::ofstream::failbit);
	}
	~file_logger_interface() {
	}

	void log(int level, const char *msg)
	{
		(void) level;
		char str[64];
		struct tm tm;
		struct timeval tv;
		char usecs_and_id[64];

		gettimeofday(&tv, NULL);
		localtime_r((time_t *)&tv.tv_sec, &tm);
		strftime(str, sizeof(str), "%F %R:%S", &tm);

		snprintf(usecs_and_id, sizeof(usecs_and_id), ".%06lu %ld/%d : ", tv.tv_usec, get_thread_id(), getpid());

		if (m_stream) {
			size_t len = strlen(msg);
			m_stream << str << usecs_and_id;
			if (len > 0 && msg[len - 1] == '\n')
				m_stream.write(msg, len - 1);
			else
				m_stream.write(msg, len);
			 m_stream << std::endl;
		} else {
			std::cerr << str << usecs_and_id << ": could not write log in elliptics file logger" << std::endl;
		}
	}

private:
	std::ofstream	m_stream;
};

class logger_data
{
public:
	logger_data(int level) : level(level) {}

	std::unique_ptr<logger_interface> impl;
	int level;
};

logger::logger() : m_data(std::make_shared<logger_data>(0))
{
}

logger::logger(logger_interface *impl, int level) : m_data(std::make_shared<logger_data>(level))
{
	m_data->impl.reset(impl);
}

logger::logger(const char *file, int level) : m_data(std::make_shared<logger_data>(level))
{
	m_data->impl.reset(new file_logger_interface(file));
}

logger::~logger()
{
}

int logger::get_level() const
{
	return m_data->level;
}

void logger::set_level(int level)
{
	m_data->level = level;
}

void logger::log(int level, const char *format, ...)
{
	if (!m_data->impl || m_data->level < level)
		return;

	va_list args;
	char buffer[1024];
	const size_t buffer_size = sizeof(buffer);

	va_start(args, format);

	vsnprintf(buffer, buffer_size, format, args);
	buffer[buffer_size - 1] = '\0';
	m_data->impl->log(level, buffer);

	va_end(args);
}

} // namespace swarm
} // namespace ioremap
