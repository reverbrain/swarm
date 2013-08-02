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

#include <string>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"

#include <thevoid/rapidjson/document.h>
#pragma GCC diagnostic pop

namespace ioremap {
namespace swarm { class logger; }
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

class base_server : private boost::noncopyable
{
public:
	base_server();
	virtual ~base_server();

	void listen(const std::string &host);
	int run(int argc, char **argv);

	void set_logger(const swarm::logger &logger);
	template <typename... Args>
	void set_logger(Args &&...args)
	{
		set_logger(swarm::logger(args...));
	}
	swarm::logger get_logger() const;

	virtual bool initialize(const rapidjson::Value &config) = 0;

protected:
	void on(const std::string &url, const std::shared_ptr<base_stream_factory> &factory);
	void on_prefix(const std::string &url, const std::shared_ptr<base_stream_factory> &factory);

private:
	template <typename Server, typename... Args>
	friend std::shared_ptr<Server> ioremap::thevoid::create_server(Args &&...args);
	template <typename T> friend class connection;

	void set_server(const std::weak_ptr<base_server> &server);
	std::shared_ptr<base_stream_factory> get_factory(const std::string &url);

	server_data *m_data;
};

template <typename Server>
class server : public base_server, public std::enable_shared_from_this<Server>
{
public:
	server() {}
	~server() {}

protected:
	void on(const std::string &url, const std::shared_ptr<base_stream_factory> &factory)
	{
		base_server::on(url, factory);
	}

	void on_prefix(const std::string &url, const std::shared_ptr<base_stream_factory> &factory)
	{
		base_server::on_prefix(url, factory);
	}

	template <typename T>
	void on(const std::string &url)
	{
		on(url, std::make_shared<stream_factory<Server, T>>(this->shared_from_this()));
	}

	template <typename T>
	void on_prefix(const std::string &url)
	{
		on_prefix(url, std::make_shared<stream_factory<Server, T>>(this->shared_from_this()));
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
