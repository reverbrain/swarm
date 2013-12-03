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
#include <sys/uio.h>

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

static const char *log_level_names[] = {
	"DATA  ",
	"ERROR ",
	"INFO  ",
	"NOTICE",
	"DEBUG "
};
static const size_t log_level_names_size = sizeof(log_level_names) / sizeof(log_level_names[0]);

class file_logger_interface : public logger_interface
{
public:
	file_logger_interface(const char *file) : m_path(file), m_file(NULL) {
		reopen();
	}
	~file_logger_interface() {
	}

	void log(int level, const char *msg)
	{
		log_internal(m_file, level, msg);
	}

	void log_internal(FILE *file, int level, const char *msg)
	{
		char str[64];
		struct tm tm;
		struct timeval tv;
		char usecs_and_id[64];

		gettimeofday(&tv, NULL);
		localtime_r((time_t *)&tv.tv_sec, &tm);
		strftime(str, sizeof(str), "%F %R:%S", &tm);
		const char *level_name = log_level_names[std::max(0, std::min<int>(level, log_level_names_size - 1))];

		snprintf(usecs_and_id, sizeof(usecs_and_id), ".%06ld %ld/%d [%s]: ",
			(long)tv.tv_usec, get_thread_id(), getpid(), level_name);

		if (!file) {
			std::cerr << str << usecs_and_id << ": could not write log in elliptics file logger" << std::endl;
			return;
		}

		const size_t msg_len = ::strlen(msg);
		const bool has_new_line = (msg_len > 0 && msg[msg_len - 1] == '\n');
		char new_line[] = "\n";

		const iovec io[] = {
			{ str, ::strlen(str) },
			{ usecs_and_id, ::strlen(usecs_and_id) },
			{ const_cast<char *>(msg), has_new_line ? msg_len  - 1 : msg_len },
			{ new_line, 1 }
		};

		if (-1 == ::writev(::fileno(file), io, sizeof(io) / sizeof(io[0]))) {
			// TODO: What to do there?
		}
		::fflush(file);
	}

	void reopen()
	{
		FILE *old_file = m_file;
		FILE *new_file = std::fopen(m_path.c_str(), "a");
		if (!new_file) {
			int err = errno;
			std::string message = "Failed to open log file \"";
			message += m_path;
			message += "\": ";
			message += strerror(err);
			throw std::ios_base::failure(message);
		}

		log_internal(new_file, -1, "Reopened log file");
		m_file = new_file;

		if (old_file) {
			log_internal(old_file, -1, "Reopened log file");
			std::fclose(old_file);
		}
	}

private:
	const std::string m_path;
	FILE *m_file;
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

void logger::reopen()
{
	if (m_data->impl) {
		m_data->impl->reopen();
	}
}

void logger::log(int level, const char *format, ...) const
{
	va_list args;
	va_start(args, format);

	vlog(level, format, args);

	va_end(args);
}

void logger::vlog(int level, const char *format, va_list args) const
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
