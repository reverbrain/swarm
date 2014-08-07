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

#include "server.hpp"
#include "stream_p.hpp"
#include <boost/regex.hpp>

namespace ioremap {
namespace thevoid {

class server_options_private
{
public:
	enum flag : uint64_t {
		check_nothing           = 0x00,
		check_methods           = 0x01,
		check_exact_match       = 0x02,
		check_prefix_match      = 0x04,
		check_string_match      = 0x08,
		check_regexp_match      = 0x10,
		check_headers           = 0x20,
		check_all_match         = check_exact_match | check_prefix_match | check_string_match | check_regexp_match,
		check_min_path_components   = 0x0040,
		check_exact_path_components = 0x0080,
		check_max_path_components   = 0x0100,
		check_all_path_components   = check_min_path_components | check_exact_path_components | check_max_path_components,
		check_host_suffix           = 0x0200,
		check_host_exact            = 0x0400,
		check_host_all              = check_host_suffix | check_host_exact,
		check_query                 = 0x0800
	};

	server_options_private() : flags(check_nothing), path_components_count(0)
	{
	}

	uint64_t flags;
	std::string match_string;
	boost::regex match_regex;
	std::vector<std::string> methods;
	std::vector<swarm::headers_entry> headers;
	std::string host_string;
	size_t path_components_count;
	std::vector<std::pair<std::string, boost::optional<std::string>>> queries;
};

base_server::options::modificator base_server::options::exact_match(const std::string &str)
{
	return std::bind(&base_server::options::set_exact_match, std::placeholders::_1, str);
}

base_server::options::modificator base_server::options::prefix_match(const std::string &str)
{
	return std::bind(&base_server::options::set_prefix_match, std::placeholders::_1, str);
}

base_server::options::modificator base_server::options::regex_match(const std::string &str)
{
	return std::bind(&base_server::options::set_regex_match, std::placeholders::_1, str);
}

base_server::options::modificator base_server::options::methods(const std::vector<std::string> &methods)
{
	return std::bind(&base_server::options::set_methods, std::placeholders::_1, methods);
}

base_server::options::modificator base_server::options::header(const std::string &name, const std::string &value)
{
	return std::bind(&base_server::options::set_header, std::placeholders::_1, name, value);
}

base_server::options::modificator base_server::options::minimal_path_components_count(size_t count)
{
	return std::bind(&base_server::options::set_minimal_path_components_count, std::placeholders::_1, count);
}

base_server::options::modificator base_server::options::exact_path_components_count(size_t count)
{
	return std::bind(&base_server::options::set_exact_path_components_count, std::placeholders::_1, count);
}

base_server::options::modificator base_server::options::maximal_path_components_count(size_t count)
{
	return std::bind(&base_server::options::set_maximal_path_components_count, std::placeholders::_1, count);
}

base_server::options::modificator base_server::options::query(const std::string &key)
{
	return std::bind(static_cast<void (base_server::options::*)(const std::string &)>(&base_server::options::set_query), std::placeholders::_1, key);
}

base_server::options::modificator base_server::options::query(const std::string &key, const std::string &value)
{
	return std::bind(static_cast<void (base_server::options::*)(const std::string &, const std::string &)>(&base_server::options::set_query), std::placeholders::_1, key, value);
}

base_server::options::modificator base_server::options::host_exact(const std::string &host)
{
	return std::bind(&base_server::options::set_host_exact, std::placeholders::_1, host);
}

base_server::options::modificator base_server::options::host_suffix(const std::string &host)
{
	return std::bind(&base_server::options::set_host_suffix, std::placeholders::_1, host);
}

base_server::options::options() : m_data(new server_options_private)
{
}

base_server::options::options(options &&other) SWARM_NOEXCEPT : m_data(std::move(other.m_data))
{
}

base_server::options &base_server::options::operator =(options &&other)
{
	m_data = std::move(other.m_data);
	return *this;
}

base_server::options::~options()
{
}

void base_server::options::set_exact_match(const std::string &str)
{
	if (m_data->flags & server_options_private::check_all_match) {
		throw std::runtime_error("trying to set_exact_match(" + str + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_exact_match | server_options_private::check_string_match;
	m_data->match_string = str;
}

void base_server::options::set_prefix_match(const std::string &str)
{
	if (m_data->flags & server_options_private::check_all_match) {
		throw std::runtime_error("trying to set_prefix_match(" + str + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_prefix_match | server_options_private::check_string_match;
	m_data->match_string = str;
}

void base_server::options::set_regex_match(const std::string &str)
{
	if (m_data->flags & server_options_private::check_all_match) {
		throw std::runtime_error("trying to set_regex_match(" + str + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_regexp_match | server_options_private::check_string_match;
	m_data->match_regex.assign(str);
}

void base_server::options::set_methods(const std::vector<std::string> &methods)
{
	m_data->flags |= server_options_private::check_methods;
	m_data->methods = methods;
}

void base_server::options::set_header(const std::string &name, const std::string &value)
{
	m_data->flags |= server_options_private::check_headers;
	m_data->headers.emplace_back(name, value);
}

void base_server::options::set_minimal_path_components_count(size_t count)
{
	if (m_data->flags & server_options_private::check_all_path_components) {
		throw std::runtime_error("trying to set_minimal_path_components_count, while another was already set");
	}
	m_data->flags |= server_options_private::check_min_path_components;
	m_data->path_components_count = count;
}

void base_server::options::set_exact_path_components_count(size_t count)
{
	if (m_data->flags & server_options_private::check_all_path_components) {
		throw std::runtime_error("trying to set_exact_path_components_count, while another was already set");
	}
	m_data->flags |= server_options_private::check_exact_path_components;
	m_data->path_components_count = count;
}

void base_server::options::set_maximal_path_components_count(size_t count)
{
	if (m_data->flags & server_options_private::check_all_path_components) {
		throw std::runtime_error("trying to set_maximal_path_components_count, while another was already set");
	}
	m_data->flags |= server_options_private::check_max_path_components;
	m_data->path_components_count = count;
}

void base_server::options::set_query(const std::string &key)
{
	m_data->flags |= server_options_private::check_query;
	m_data->queries.emplace_back(key, boost::none);
}

void base_server::options::set_query(const std::string &key, const std::string &value)
{
	m_data->flags |= server_options_private::check_query;
	m_data->queries.emplace_back(key, value);
}

void base_server::options::set_host_exact(const std::string &host)
{
	if (m_data->flags & server_options_private::check_host_all) {
		throw std::runtime_error("trying to set_host_exact(" + host + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_host_exact;
	m_data->host_string = host;
}

void base_server::options::set_host_suffix(const std::string &host)
{
	if (m_data->flags & server_options_private::check_host_all) {
		throw std::runtime_error("trying to set_host_suffix(" + host + "), while another was already set");
	}
	m_data->flags |= server_options_private::check_host_suffix;
	m_data->host_string = host;
}

bool base_server::options::check(const http_request &request) const
{
	if (m_data->flags & server_options_private::check_methods) {
		const auto &methods = m_data->methods;
		if (std::find(methods.begin(), methods.end(), request.method()) == methods.end())
			return false;
	}

	if (m_data->flags & server_options_private::check_all_path_components) {
		const size_t path_components_count = request.url().path_components().size();

		switch (m_data->flags & server_options_private::check_all_path_components) {
			case server_options_private::check_min_path_components:
				if (path_components_count < m_data->path_components_count)
					return false;
				break;
			case server_options_private::check_exact_path_components:
				if (path_components_count != m_data->path_components_count)
					return false;
				break;
			case server_options_private::check_max_path_components:
				if (path_components_count > m_data->path_components_count)
					return false;
				break;
			default:
				break;
		}
	}

	if (m_data->flags & server_options_private::check_string_match) {
		const std::string &match = m_data->match_string;

		if (m_data->flags & server_options_private::check_exact_match) {
			if (match != request.url().path()) {
				return false;
			}
		} else if (m_data->flags & server_options_private::check_prefix_match) {
			if (request.url().path().compare(0, match.size(), match) != 0) {
				return false;
			}
		} else if (m_data->flags & server_options_private::check_regexp_match) {
			if (!boost::regex_match(request.url().path(), m_data->match_regex)) {
				return false;
			}
		}
	}

	if (m_data->flags & server_options_private::check_host_all) {
		const auto &host_ptr = request.headers().get("Host");
		if (!host_ptr) {
			return false;
		}
		const std::string &host = *host_ptr;
		// Remove port from 'Host: domain.com:8080'
		const size_t host_size = std::min(host.size(), host.find_first_of(':'));

		if (m_data->flags & server_options_private::check_host_exact) {
			if (host.compare(0, host_size, m_data->host_string) != 0) {
				return false;
			}
		} else if (m_data->flags & server_options_private::check_host_suffix) {
			if (host_size < m_data->host_string.size()) {
				return false;
			}
			if (host.compare(host_size - m_data->host_string.size(), m_data->host_string.size(), m_data->host_string) != 0) {
				return false;
			}
		}
	}

	if (m_data->flags & server_options_private::check_query) {
		const auto &query = request.url().query();
		const auto &queries = m_data->queries;
		for (auto it = queries.begin(); it != queries.end(); ++it) {
			auto value = query.item_value(it->first);
			if (!value) {
				return false;
			}

			if (!it->second) {
				// We just want to check if such query parameter exists, we don't care about the exact value
				continue;
			}

			if (*it->second != *value) {
				// Value mismatch
				return false;
			}
		}
	}

	if (m_data->flags & server_options_private::check_headers) {
		const auto &request_headers = request.headers();
		const auto &headers = m_data->headers;
		for (auto it = headers.begin(); it != headers.end(); ++it) {
			if (auto value = request_headers.get(it->first)) {
				if (*value != it->second) {
					return false;
				}
			} else {
				return false;
			}
		}
	}

	return true;
}

void base_server::options::swap(base_server::options &other)
{
	using std::swap;
	swap(m_data, other.m_data);
}

} } // namespace ioremap::thevoid
