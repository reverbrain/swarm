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

// For BigBang
#include <blackhole/repository.hpp>

#include <blackhole/sink/files.hpp>
#include <blackhole/formatter/string.hpp>
#include <blackhole/frontend/files.hpp>

#include <boost/io/ios_state.hpp>

namespace ioremap {
namespace swarm {
namespace utils {
namespace logger {

blackhole::attribute::set_t default_attributes()
{
	return blackhole::attribute::set_t {
		keyword::request_id() = 0
	};
}

void init_attributes(logger_base &log)
{
	blackhole::attribute::set_t attributes = default_attributes();

	for (auto it = attributes.begin(); it != attributes.end(); ++it) {
		log.add_attribute(*it);
	}
}

void add_file_frontend(logger_base &log, const std::string &file, log_level level)
{
	log.verbosity(level);

	std::unique_ptr<blackhole::formatter::string_t> formatter(new blackhole::formatter::string_t(format()));
	formatter->set_mapper(mapping());

	std::unique_ptr<blackhole::sink::files_t<>> sink(new blackhole::sink::files_t<>(blackhole::sink::files_t<>::config_type(file)));

	typedef blackhole::frontend_t<
		blackhole::formatter::string_t,
		blackhole::sink::files_t<>
	> frontend_type;
	std::unique_ptr<frontend_type>frontend(new frontend_type(std::move(formatter), std::move(sink)));

	log.add_frontend(std::move(frontend));
}

std::string format()
{
	return "%(timestamp)s %(request_id)s/%(tid)s/%(pid)s %(severity)s: %(message)s, attrs: [%(...L)s]";
}

static const char *severity_names[] = {
	"debug",
	"notice",
	"info",
	"warning",
	"error"
};
const size_t severity_names_count = sizeof(severity_names) / sizeof(severity_names[0]);

std::string generate_level(log_level level)
{
	typedef blackhole::aux::underlying_type<log_level>::type level_type;
	auto value = static_cast<level_type>(level);

	if (value < 0 || value >= static_cast<level_type>(severity_names_count)) {
		return "unknown";
	}

	return severity_names[value];
}

log_level parse_level(const std::string &name)
{
	auto it = std::find(severity_names, severity_names + severity_names_count, name);
	if (it == severity_names + severity_names_count) {
		throw std::logic_error("Unknown log level: " + name);
	}

	return static_cast<log_level>(it - severity_names);
}

static void format_request_id(blackhole::aux::attachable_ostringstream &out, uint64_t request_id)
{
	boost::io::ios_flags_saver ifs(out);
	out << std::setw(16) << std::setfill('0') << std::hex << request_id;
}

struct localtime_formatter_action {
	blackhole::aux::datetime::generator_t generator;

	localtime_formatter_action(const std::string &format) :
	generator(blackhole::aux::datetime::generator_factory_t::make(format))
	{
	}

	void operator() (blackhole::aux::attachable_ostringstream &stream, const timeval &value) const
	{
	std::tm tm;
	localtime_r(&value.tv_sec, &tm);
	generator(stream, tm, value.tv_usec);
	}
};

blackhole::mapping::value_t mapping()
{
	blackhole::mapping::value_t mapper;
	mapper.add<blackhole::keyword::tag::timestamp_t>(localtime_formatter_action("%Y-%m-%d %H:%M:%S.%f"));
	mapper.add<keyword::tag::request_id_t>(format_request_id);
	mapper.add<blackhole::keyword::tag::severity_t<log_level>>(blackhole::defaults::map_severity);
	return mapper;
}

logger_base create(const std::string &file, log_level level)
{
	logger_base logger;
	init_attributes(logger);
	add_file_frontend(logger, file, level);
	return logger;
}

} } } } // namespace ioremap::swarm::utils::logger
