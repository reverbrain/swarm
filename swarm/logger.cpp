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

#include <blackhole/log.hpp>
#include <blackhole/logger.hpp>
#include <blackhole/repository.hpp>

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

class blackhole_logger_interface : public logger_interface {
	blackhole::verbose_logger_t<log_level> m_log;
public:
	blackhole_logger_interface(const blackhole::log_config_t &config) :
		m_log(init_from_config(config))
	{
	}

	void log(int level, const char *msg) {
		BH_LOG(m_log, static_cast<log_level>(level), msg);
	}

	void reopen() {
		// Do nothing ATM, because logger can rotate itself.
	}

private:
	static blackhole::verbose_logger_t<log_level> init_from_config(const blackhole::log_config_t &config) {
		blackhole::log_config_t copy = config;

		blackhole::mapping::value_t mapper;
		mapper.add<blackhole::keyword::tag::severity_t<log_level>>(&blackhole_logger_interface::map_severity);
		mapper.add<blackhole::keyword::tag::timestamp_t>("%Y-%m-%d %H:%M:%S.%f");
		for (auto it = copy.frontends.begin(); it != copy.frontends.end(); ++it) {
			it->formatter.mapper = mapper;
		}

		auto& repository = blackhole::repository_t::instance();
		repository.add_config(copy);
		return repository.create<log_level>(copy.name);
	}

	static void map_severity(blackhole::aux::attachable_ostringstream& stream, const log_level& lvl) {
		auto value = static_cast<blackhole::aux::underlying_type<log_level>::type>(lvl);
		if (value < log_level_names_size) {
			stream << log_level_names[value];
		} else {
			stream << lvl;
		}
	}
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

logger::logger(const blackhole::log_config_t &config, int level)
	: m_data(std::make_shared<logger_data>(level))
{
	m_data->impl.reset(new blackhole_logger_interface(config));
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
