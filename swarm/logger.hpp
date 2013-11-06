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

namespace ioremap {
namespace swarm {

class logger_interface
{
public:
	virtual ~logger_interface() {}
	virtual void log(int level, const char *msg) = 0;
};

enum log_level {
	SWARM_LOG_DATA = 0,
	SWARM_LOG_ERROR = 1,
	SWARM_LOG_INFO = 2,
	SWARM_LOG_NOTICE = 3,
	SWARM_LOG_DEBUG = 4
};

class logger_data;

class logger
{
public:
	logger();
	logger(logger_interface *impl, int level);
	logger(const char *file, int level);
	~logger();

	int level() const;
	void set_level(int level);

	void log(int level, const char *format, ...) __attribute__ ((format(printf, 3, 4)));
	void vlog(int level, const char *format, va_list args);

private:
	std::shared_ptr<logger_data> m_data;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_LOGGER_H
