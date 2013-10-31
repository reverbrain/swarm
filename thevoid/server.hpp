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

//
// server.hpp
// ~~~~~~~~~~
/*
 * 2013+ Copyright (c) Ruslan Nigatullin <euroelessar@yandex.ru>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef IOREMAP_THEVOID_SERVER_HPP
#define IOREMAP_THEVOID_SERVER_HPP

#include "streamfactory.hpp"

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>

#include <string>
#include <vector>

#ifdef __GNUC__
#pragma GCC diagnostic push
#if __GNUC__ > 5 || (__GNUC__ == 4 && __GNUC_MINOR__ > 6)
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#endif

#include <thevoid/rapidjson/document.h>

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace ioremap {
namespace swarm {
class logger;
class url;
}
namespace thevoid {

namespace detail {

template<typename Parent, typename Type>
struct check {
private:
	typedef char true_type;
	class false_type { char arr[2]; };

	static Type create();

	static true_type process(Parent);
	static false_type process(...);
public:
	enum {
		result = (sizeof(process(create())) == sizeof(true_type))
	};
};

}

class server_data;
template <typename T> class connection;
class monitor_connection;
class server_options_private;

class base_server : private boost::noncopyable
{
public:
	base_server();
	virtual ~base_server();

	void listen(const std::string &host);
	int run(int argc, char **argv);

	void set_logger(const swarm::logger &logger);
	swarm::logger logger() const;

	virtual std::map<std::string, std::string> get_statistics() const;

	unsigned int threads_count() const;

	virtual bool initialize(const rapidjson::Value &config) = 0;
	virtual bool initialize_logger(const rapidjson::Value &config);

protected:
	class options
	{
	public:
		class exact_match : private boost::noncopyable
		{
		public:
			exact_match(const std::string &str) : m_str(str)
			{
			}

			void apply(options &opt)
			{
				opt.set_exact_match(m_str);
			}

		private:
			std::string m_str;
		};

		class prefix_match : private boost::noncopyable
		{
		public:
			prefix_match(const std::string &str) : m_str(str)
			{
			}

			void apply(options &opt)
			{
				opt.set_prefix_match(m_str);
			}

		private:
			std::string m_str;
		};

		class methods : private boost::noncopyable
		{
		public:
			enum special_value {
				all
			};

			methods(special_value value) : m_methods(value)
			{
			}

			template <typename... String>
			methods(String &&...args) : m_methods(std::vector<std::string>{ std::forward<std::string>(args)... })
			{
			}

			void apply(options &opt)
			{
				if (auto tmp = boost::get<std::vector<std::string>>(&m_methods)) {
					opt.set_methods(*tmp);
				} else if (auto tmp = boost::get<special_value>(&m_methods)) {
					opt.set_methods(*tmp);
				}
			}

		private:
			boost::variant<std::vector<std::string>, special_value> m_methods;
		};

		options();

		options(options &&other);
		options(const options &other) = delete;
		options &operator =(options &&other);
		options &operator =(const options &other) = delete;

		~options();

		void set_exact_match(const std::string &str);
		void set_prefix_match(const std::string &str);
		void set_methods(const std::vector<std::string> &methods);
		void set_methods(methods::special_value value);

		bool check(const swarm::http_request &request) const;

		void swap(options &other);

	private:
		std::unique_ptr<server_options_private> m_data;
	};

	void on(options &&opts, const std::shared_ptr<base_stream_factory> &factory);

private:
	template <typename Server, typename... Args>
	friend std::shared_ptr<Server> ioremap::thevoid::create_server(Args &&...args);
	template <typename T> friend class connection;
	friend class monitor_connection;
	friend class server_data;

	void set_server(const std::weak_ptr<base_server> &server);
	std::shared_ptr<base_stream_factory> factory(const swarm::http_request &request);

	std::unique_ptr<server_data> m_data;
};

template <typename Server>
class server : public base_server, public std::enable_shared_from_this<Server>
{
public:
	server() {}
	~server() {}

protected:
	template <typename T, typename... Options>
	void on(Options &&...args)
	{
		options opts;
		options_pass(apply_option(opts, args)...);
		base_server::on(std::move(opts), std::make_shared<stream_factory<Server, T>>(this->shared_from_this()));
	}

private:
	template <typename Option>
	Option &&apply_option(options &opt, Option &&option)
	{
		option.apply(opt);
		return std::forward<Option>(option);
	}

	template <typename... Options>
	void options_pass(Options &&...)
	{
	}
};

template <typename Server, typename... Args>
std::shared_ptr<Server> create_server(Args &&...args)
{
	auto server = std::make_shared<Server>(std::forward<Args>(args)...);
	server->set_server(server);
	return server;
}

template <typename Server, typename... Args>
int run_server(int argc, char **argv, Args &&...args)
{
	return create_server<Server>(std::forward<Args>(args)...)->run(argc, argv);
}

} } // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_SERVER_HPP
