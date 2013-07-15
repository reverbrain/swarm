#ifndef IOREMAP_THEVOID_ACCEPTORLIST_P_HPP
#define IOREMAP_THEVOID_ACCEPTORLIST_P_HPP

#include "server.hpp"
#include <boost/thread.hpp>

namespace ioremap {
namespace thevoid {

template <typename Connection>
class acceptors_list
{
public:
	typedef typename Connection::socket_type socket_type;
	typedef typename socket_type::protocol_type protocol_type;
	typedef typename protocol_type::endpoint endpoint_type;
	typedef boost::asio::basic_socket_acceptor<protocol_type> acceptor_type;
	typedef Connection connection_type;
	typedef std::shared_ptr<connection_type> connection_ptr_type;

	acceptors_list(server_data &data) : data(data)
	{
	}

	~acceptors_list() {}

	void add_acceptor(const std::string &address);
	void start_acceptor(size_t index);
	void handle_accept(size_t index, const connection_ptr_type &conn, const boost::system::error_code &err);

	void start_threads(int thread_count, std::vector<boost::thread> &threads);

	void handle_stop();

	endpoint_type create_endpoint(acceptor_type &acc, const std::string &host);

	server_data &data;
	std::vector<std::unique_ptr<acceptor_type>> acceptors;
	std::vector<std::unique_ptr<boost::asio::io_service>> io_services;
};

} }

#endif // IOREMAP_THEVOID_ACCEPTORLIST_P_HPP
