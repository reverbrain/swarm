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

#ifndef BOOST_BIND_NO_PLACEHOLDERS
# define BOOST_BIND_NO_PLACEHOLDERS
# define BOOST_BIND_NO_PLACEHOLDERS_SET_BY_SWARM
#endif

#include <blackhole/log.hpp>
#include <blackhole/logger/wrapper.hpp>
#include <blackhole/formatter/map/value.hpp>
#include <blackhole/defaults/severity.hpp>

#ifdef BOOST_BIND_NO_PLACEHOLDERS_SET_BY_SWARM
# undef BOOST_BIND_NO_PLACEHOLDERS_SET_BY_SWARM
# undef BOOST_BIND_NO_PLACEHOLDERS
#endif

#define SWARM_LOG_ERROR blackhole::defaults::severity::error
#define SWARM_LOG_WARNING blackhole::defaults::severity::warning
#define SWARM_LOG_INFO blackhole::defaults::severity::info
#define SWARM_LOG_NOTICE blackhole::defaults::severity::notice
#define SWARM_LOG_DEBUG blackhole::defaults::severity::debug

namespace ioremap {
namespace swarm {

typedef blackhole::defaults::severity log_level;
typedef blackhole::verbose_logger_t<log_level> logger_base;
typedef blackhole::wrapper_t<logger_base> logger;

namespace utils {
namespace logger {

blackhole::attribute::set_t default_attributes();
void init_attributes(logger_base &log);
void add_file_frontend(logger_base &log, const std::string &file, log_level level);
std::string format();
std::string generate_level(log_level level);
log_level parse_level(const std::string &name);
blackhole::mapping::value_t mapping();

logger_base create(const std::string &file, log_level level);

} } // namespace utils::logger

DECLARE_EVENT_KEYWORD(request_id, uint64_t)
DECLARE_LOCAL_KEYWORD(source, std::string)
DECLARE_LOCAL_KEYWORD(url, std::string)

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_LOGGER_H
