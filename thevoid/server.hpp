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

#ifndef IOREMAP_THEVOID_SERVER_HPP
#define IOREMAP_THEVOID_SERVER_HPP

#include "streamfactory.hpp"

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/variant.hpp>
#include <swarm/c++config.hpp>

#include <string>
#include <vector>

#if !defined(__clang__) && !defined(SWARM_GCC_4_4)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

#include <thevoid/rapidjson/document.h>

#if !defined(__clang__) && !defined(SWARM_GCC_4_4)
#pragma GCC diagnostic pop
#endif

namespace ioremap {
namespace swarm {
class logger;
class url;
}
namespace thevoid {

class server_data;
template <typename T> class connection;
class monitor_connection;
class server_options_private;

/*!
 * \brief The daemon_exception is thrown in case if daemonization fails.
 */
class daemon_exception : public std::runtime_error
{
public:
	daemon_exception();
	daemon_exception(const std::string &error);
};

template <typename Server, typename... Args>
std::shared_ptr<Server> create_server(Args &&...args);

/*!
 * \brief The base_server class is a base class for implementing own HTTP servers.
 * 
 * Ususally you don't need to subclass it directly, use server for this.
 */
class base_server : private boost::noncopyable
{
public:
	/*!
	 * \brief Constructs base server.
	 */
	base_server();
	/*!
	 * \brief Destroyes the object.
	 */
	virtual ~base_server();

	/*!
	 * \brief Listen for new connections at \a host.
	 * 
	 *  There are two possible formats for \a host:
	 *  \li host:port or address:port - listen at certain interface at \a port.
	 *  \li unix:path - listen unix socket at \a path.
	 * 
	 *  \code{.cpp}
	 *  server.listen("127.0.0.1:80");
	 *  server.listen("unix:/var/run/server.sock");
	 *  \endcode
	 */
	void listen(const std::string &host);
	
	/*!
	 * \brief Run server using provided command line arguments.
	 * 
	 *  Supported arguments are:
	 *  \li help - show help message and return.
	 *  \li config path - read configuration file \a path.
	 *  \li pidfile file - write PID file to \a file.
	 */
	int run(int argc, char **argv);

	/*!
	 * \brief Ser \a logger as logger of the server.
	 */
	void set_logger(const swarm::logger &logger);
	/*!
	 * \brief Returns logger of the service.
	 */
	swarm::logger logger() const;

	/*!
	 * \brief Returns server-specific statistics as a key-value map.
	 * 
	 *  Reimplement this if you want your own statistics available.
	 */
	virtual std::map<std::string, std::string> get_statistics() const;

	/*!
	 * \brief Returns number of worker threads count. 
	 */
	unsigned int threads_count() const;

	/*!
	 * \brief Initialize server by application-specific section \a config from configuration file.
	 *
	 *  Returns true if initialization was succesfull.
	 */
	virtual bool initialize(const rapidjson::Value &config) = 0;
	/*!
	 * \brief Initialize logger by \a config.
	 * 
	 *  Override this method if you want to initialize your own logger.
	 *
	 *  Returns true if initialization was succesfull.
	 */
	virtual bool initialize_logger(const rapidjson::Value &config);

protected:
	/*!
	 * \brief The options class provides API for settings handler options.
	 *
	 * It makes possible to specify conditions at which handler should be called.
	 */
	class options
	{
	public:
		typedef std::function<void (options *)> modificator;

		/*!
		 * \brief Calls options::set_exact_match
		 *
		 * \sa set_exact_match
		 */
		static modificator exact_match(const std::string &str);
		/*!
		 * \brief Calls options::set_prefix_match
		 *
		 * \sa set_prefix_match
		 */
		static modificator prefix_match(const std::string &str);
		/*!
		 * \brief Calls options::set_methods
		 *
		 * \sa set_methods
		 */
		static modificator methods(const std::vector<std::string> &methods);
		/*!
		 * \brief Calls options::set_methods
		 *
		 * Creates std::vector from \a args and passes it to set_methods.
		 *
		 * \sa set_methods
		 */
		template <typename... String>
		static modificator methods(String &&...args)
		{
			const std::vector<std::string> tmp = { std::forward<std::string>(args)... };
			return methods(tmp);
		}
		/*!
		 * \brief Calls options::set_header
		 *
		 * \sa set_header
		 */
		static modificator header(const std::string &name, const std::string &value);

		/*!
		 * \brief Constructs options object.
		 */
		options();

		options(options &&other) SWARM_NOEXCEPT;
		options(const options &other) = delete;
		options &operator =(options &&other);
		options &operator =(const options &other) = delete;
		/*!
		 * Destroyes the object.
		 */
		~options();

		/*!
		 * \brief Makes handler callable only if path of the request is exactly equal to \a str.
		 */
		void set_exact_match(const std::string &str);
		/*!
		 * \brief Makes handler callable only if path of the request starts with \a str.
		 */
		void set_prefix_match(const std::string &str);
		/*!
		 * \brief Makes handler callable if HTTP method is one of \a methods.
		 */
		void set_methods(const std::vector<std::string> &methods);
		/*!
		 * \brief Makes handler callable if HTTP header \a name is equal to \a value.
		 */
		void set_header(const std::string &name, const std::string &value);

		/*!
		 * \brief Returns true if request satisfies all conditions.
		 */
		bool check(const swarm::http_request &request) const;

		/*!
		 * \brief Swaps this options with \a other.
		 */
		void swap(options &other);

	private:
		std::unique_ptr<server_options_private> m_data;
	};

	/*!
	 * \brief Registers handler producable by \a factory with options \a opts.
	 */
	void on(options &&opts, const std::shared_ptr<base_stream_factory> &factory);

	/*!
	 * \internal
	 *
	 * \brief Daemonize the server.
	 *
	 * Exception is thrown on error.
	 */
	void daemonize();

private:
	template <typename Server, typename... Args>
	friend std::shared_ptr<Server> ioremap::thevoid::create_server(Args &&...args);
	template <typename T> friend class connection;
	friend class monitor_connection;
	friend class server_data;

	/*!
	 * \internal
	 */
	void set_server(const std::weak_ptr<base_server> &server);
	/*!
	 * \internal
	 */
	std::shared_ptr<base_stream_factory> factory(const swarm::http_request &request);

	std::unique_ptr<server_data> m_data;
};

/*!
 * \brief The server provides API for constructing own HTTP server.
 *
 * This is a base class you really should derive from.
 */
template <typename Server>
class server : public base_server, public std::enable_shared_from_this<Server>
{
public:
	/*!
	 * \brief Creates server.
	 */
	server() {}
	/*!
	 * \brief Destroyes server.
	 */
	~server() {}

protected:
	/*!
	 * \brief Add new handler of type \a T with options \a args.
	 *
	 * The following example show how to handle \a on_ping on URL "/ping" for GET requests:
	 * \code{.cpp}
	 * on<on_ping>(
	 *     options::exact_match("/ping"),
	 *     options::methods("GET")
	 * );
	 * \endcode
	 *
	 * Handler must be a successor of base_request_stream.
	 *
	 * \sa options
	 */
	template <typename T, typename... Options>
	void on(Options &&...args)
	{
		options opts;
		options_pass(apply_option(opts, args)...);
		base_server::on(std::move(opts), std::make_shared<stream_factory<Server, T>>(this->shared_from_this()));
	}

private:
	/*!
	 * \internal
	 */
	template <typename Option>
	Option &&apply_option(options &opt, Option &&option)
	{
		option(&opt);
		return std::forward<Option>(option);
	}

	/*!
	 * \internal
	 */
	template <typename... Options>
	void options_pass(Options &&...)
	{
	}
};

/*!
 * \brief Creates server with \a args, they are passed to \a Server's constructor.
 * 
 *  Server is created via std::make_shared, so std::enable_shared_from_this works.
 */
template <typename Server, typename... Args>
std::shared_ptr<Server> create_server(Args &&...args)
{
	auto server = std::make_shared<Server>(std::forward<Args>(args)...);
	server->set_server(server);
	return server;
}

/*!
 * \brief Run server \a Server with \a args.
 * 
 *  It's equivalent to:
 *  \code{.cpp}
 *  auto server = create_server<Server>(args);
 *  return server->run(argc, argv);
 *  \encode
 */
template <typename Server, typename... Args>
int run_server(int argc, char **argv, Args &&...args)
{
	return create_server<Server>(std::forward<Args>(args)...)->run(argc, argv);
}

} } // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_SERVER_HPP
