#include "monitor_connection_p.hpp"
#include "server_p.hpp"

#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#include <boost/bind.hpp>

namespace ioremap {
namespace thevoid {

monitor_connection::monitor_connection(boost::asio::io_service &io_service, size_t buffer_size)
	: m_io_service(io_service),
	  m_socket(io_service)
{
	(void) buffer_size;
}

monitor_connection::~monitor_connection()
{
}

monitor_connection::socket_type &monitor_connection::socket()
{
	return m_socket;
}

void monitor_connection::start(const std::shared_ptr<base_server> &server)
{
	m_server = server;
	async_read();
}

std::string monitor_connection::get_information()
{
	auto server_statistics = m_server->get_statistics();

	rapidjson::MemoryPoolAllocator<> allocator;
	rapidjson::Value information;
	information.SetObject();

	information.AddMember("connections", int(m_server->m_data->connections_counter), allocator);
	information.AddMember("active-connections", int(m_server->m_data->active_connections_counter), allocator);

	rapidjson::Value application;
	application.SetObject();

	for (auto it = server_statistics.begin(); it != server_statistics.end(); ++it) {
		application.AddMember(it->first.c_str(), it->second.c_str(), allocator);
	}

	information.AddMember("application", application, allocator);

	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

	information.Accept(writer);
	buffer.Put('\n');

	return std::string(buffer.GetString(), buffer.GetSize());
}

void monitor_connection::async_read()
{
	m_socket.async_read_some(boost::asio::buffer(m_buffer),
				 std::bind(&monitor_connection::handle_read, shared_from_this(),
					   std::placeholders::_1,
					   std::placeholders::_2));
}

void monitor_connection::handle_read(const boost::system::error_code &err, std::size_t bytes_transferred)
{
	if (err || bytes_transferred < 1) {
		close();
		return;
	}

	switch (m_buffer[0]) {
		case 'i': case 'I':
			async_write(get_information());
			break;
		case 's': case 'S': {
			const char *result = "Stopping...\n";
			boost::asio::async_write(m_socket, boost::asio::buffer(result, strlen(result)),
				std::bind(&monitor_connection::handle_stop_write, shared_from_this(),
					std::placeholders::_1, std::placeholders::_2));
			break;
		}
		default:
		case 'h': case 'H':
			async_write("i - statistics information\n"
				    "s - stop server\n"
				    "h - this help message\n");
			break;
	}
}

void monitor_connection::async_write(const std::string &data)
{
	m_storage = data;
	boost::asio::async_write(m_socket, boost::asio::buffer(m_storage),
				 std::bind(&monitor_connection::handle_write, shared_from_this(),
					   std::placeholders::_1,
					   std::placeholders::_2));
}

void monitor_connection::handle_write(const boost::system::error_code &, size_t)
{
	close();
}

void monitor_connection::handle_stop_write(const boost::system::error_code &, size_t)
{
	close();
	m_server->m_data->handle_stop();
}

void monitor_connection::close()
{
	// gratefull shutdown
	boost::system::error_code ignored_ec;
	m_socket.shutdown(boost::asio::socket_base::shutdown_both, ignored_ec);
}

} // namespace thevoid
} // namespace ioremap
