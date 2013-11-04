/*
 * Copyright 2013+ Ruslan Nigmatullin <euroelessar@yandex.ru>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "logger.hpp"
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
#ifdef __linux__
#include <sys/syscall.h>
static long get_thread_id()
{
	return syscall(SYS_gettid);
}
#else
static long get_thread_id()
{
	return -1;
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

		snprintf(usecs_and_id, sizeof(usecs_and_id), ".%06ld %ld/%d : ",
			(long)tv.tv_usec, get_thread_id(), getpid());

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

int logger::level() const
{
	return m_data->level;
}

void logger::set_level(int level)
{
	m_data->level = level;
}

void logger::log(int level, const char *format, ...)
{
	va_list args;
	va_start(args, format);

	vlog(level, format, args);

	va_end(args);
}

void logger::vlog(int level, const char *format, va_list args)
{
	if (!m_data->impl || m_data->level < level)
		return;

	char buffer[1024];
	const size_t buffer_size = sizeof(buffer);

	vsnprintf(buffer, buffer_size, format, args);
	buffer[buffer_size - 1] = '\0';
	m_data->impl->log(level, buffer);
}

} // namespace swarm
} // namespace ioremap
