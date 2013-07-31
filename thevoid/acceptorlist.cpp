#include "acceptorlist_p.hpp"
#include "server_p.hpp"
#include "monitor_connection_p.hpp"
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <sys/stat.h>

namespace ioremap { namespace thevoid {

enum { MAX_CONNECTIONS_COUNT = 128 };

template <typename Endpoint>
static void complete_socket_creation(Endpoint endpoint)
{
	(void) endpoint;
}

static void complete_socket_creation(boost::asio::local::stream_protocol::endpoint endpoint)
{
	chmod(endpoint.path().c_str(), 0666);
}

template <typename Connection>
void acceptors_list<Connection>::add_acceptor(const std::string &address)
{
	io_services.emplace_back(new boost::asio::io_service);
	acceptors.emplace_back(new acceptor_type(*io_services.back()));

	auto &acceptor = acceptors.back();

	try {
		endpoint_type endpoint = create_endpoint(*acceptor, address);

		acceptor->open(endpoint.protocol());
		acceptor->set_option(boost::asio::socket_base::reuse_address(true));
		acceptor->bind(endpoint);
		acceptor->listen(data.backlog_size);

		complete_socket_creation(endpoint);
	} catch (boost::system::system_error &error) {
		std::cerr << "Can not bind socket \"" << address << "\": " << error.what() << std::endl;
		std::cerr.flush();
		throw;
	}

	start_acceptor(acceptors.size() - 1);
}

template <typename Connection>
void acceptors_list<Connection>::start_acceptor(size_t index)
{
	acceptor_type &acc = *acceptors[index];

	auto conn = std::make_shared<connection_type>(acc.get_io_service());

	acc.async_accept(conn->socket(), boost::bind(
				 &acceptors_list::handle_accept, this, index, conn, _1));
}

template <typename Connection>
void acceptors_list<Connection>::handle_accept(size_t index, const connection_ptr_type &conn, const boost::system::error_code &err)
{
	if (!err) {
		if (auto server = data.server.lock()) {
			conn->start(server);
		} else {
			throw std::logic_error("server::m_data->server is null");
		}
	}

	start_acceptor(index);
}

template <typename Connection>
void acceptors_list<Connection>::start_threads(int thread_count, std::vector<std::shared_ptr<boost::thread> > &threads)
{
	for (size_t i = 0; i < io_services.size(); ++i) {
		auto functor = boost::bind(&boost::asio::io_service::run, io_services[i].get());
		for (int j = 0; j < thread_count; ++j) {
			threads.emplace_back(std::make_shared<boost::thread>(functor));
		}
	}
}

template <typename Connection>
void acceptors_list<Connection>::handle_stop()
{
	for (size_t i = 0; i < io_services.size(); ++i) {
		io_services[i]->stop();
	}
}

template <typename Connection>
typename acceptors_list<Connection>::endpoint_type acceptors_list<Connection>::create_endpoint(acceptor_type &acc, const std::string &host)
{
	(void) acc;

	size_t delim = host.find_last_of(':');
	auto address = boost::asio::ip::address::from_string(host.substr(0, delim));
	auto port = boost::lexical_cast<unsigned short>(host.substr(delim + 1));

	boost::asio::ip::tcp::endpoint endpoint(address, port);

	return endpoint;
}

template <>
acceptors_list<unix_connection>::~acceptors_list()
{
	for (size_t i = 0; i < acceptors.size(); ++i) {
		auto &acceptor = *acceptors[i];
		auto path = acceptor.local_endpoint().path();
		unlink(path.c_str());
	}
}

template <>
acceptors_list<unix_connection>::endpoint_type acceptors_list<unix_connection>::create_endpoint(acceptor_type &acc,
		const std::string &host)
{
	(void) acc;
	return boost::asio::local::stream_protocol::endpoint(host);
}

template class acceptors_list<tcp_connection>;
template class acceptors_list<unix_connection>;
template class acceptors_list<monitor_connection>;

} }
