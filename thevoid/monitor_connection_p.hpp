#ifndef IOREMAP_THEVOID_MONITOR_CONNECTION_P_HPP
#define IOREMAP_THEVOID_MONITOR_CONNECTION_P_HPP

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include "server.hpp"

namespace ioremap {
namespace thevoid {

class monitor_connection : public std::enable_shared_from_this<monitor_connection>
{
public:
	typedef boost::asio::ip::tcp::socket socket_type;

	monitor_connection(boost::asio::io_service &io_service, size_t buffer_size);
	~monitor_connection();

	socket_type &socket();

	void start(const std::shared_ptr<base_server> &server);

protected:
	std::string get_information();
	void async_read();
	void handle_read(const boost::system::error_code &err, std::size_t bytes_transferred);
	void async_write(const std::string &data);
	void handle_write(const boost::system::error_code &err, size_t);
    void handle_stop_write(const boost::system::error_code &err, size_t);
	void close();

private:
	boost::asio::io_service &m_io_service;
    std::shared_ptr<base_server> m_server;
	socket_type m_socket;
	boost::array<char, 64> m_buffer;
	std::string m_storage;
};

} // namespace thevoid
} // namespace ioremap

#endif // IOREMAP_THEVOID_MONITOR_CONNECTION_P_HPP
