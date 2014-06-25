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

#ifndef IOREMAP_SWARM_LOGGER_H
#define IOREMAP_SWARM_LOGGER_H

#include <memory>
#include <cstdarg>


namespace blackhole {
struct log_config_t;
} // namespace blackhole

namespace ioremap {
namespace swarm {

class logger_interface
{
public:
	virtual ~logger_interface() {}
	virtual void log(int level, const char *msg) = 0;
	virtual void reopen() = 0;
};

enum log_level {
	SWARM_LOG_DATA = 0,
	SWARM_LOG_ERROR = 1,
	SWARM_LOG_INFO = 2,
	SWARM_LOG_NOTICE = 3,
	SWARM_LOG_DEBUG = 4
};

class logger_data;

/*!
 * \brief The logger class is convient class for logging facility.
 *
 * Logging level may be changed at any time by \a set_level.
 * In case of file logger file may be reopened by calling \a reopen method.
 *
 * It is explicitly shared object.
 */
class logger
{
public:
	/*!
	 * \brief Constructs a null logger.
	 */
	logger();
	/*!
	 * \brief Constructs logger from implementation \a impl with \a level.
	 */
	logger(logger_interface *impl, int level);
	/*!
	 * \brief Constructs file logger with \a level.
	 *
	 * Logger will write all entries to \a file.
	 */
	logger(const char *file, int level);

	/*!
	 * \brief Constructs Blackhole logger with \a level.
	 *
	 * Logger will write all entries to Blackhole as specified in its \a config.
	 */
	logger(const blackhole::log_config_t &config, int level);

	/*!
	 * Destroyes object.
	 */
	~logger();

	/*!
	 * \brief Returnes level of the logger.
	 */
	int level() const;
	/*!
	 * \brief Set level of the logger to the \a level.
	 */
	void set_level(int level);

	/*!
	 * \brief Reopens logger.
	 *
	 * If logger is not file one this method may have no effect.
	 */
	void reopen();

	/*!
	 * \brief Logs message \a format with \a level.
	 *
	 * \attention This method uses printf-like notation.
	 */
	void log(int level, const char *format, ...) const __attribute__ ((format(printf, 3, 4)));
	/*!
	 * \overload
	 */
	void vlog(int level, const char *format, va_list args) const;

private:
	std::shared_ptr<logger_data> m_data;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_LOGGER_H
