#include "http_request.hpp"

#include <algorithm>

namespace ioremap {
namespace thevoid {

class http_request_data
{
public:
	http_request_data() :
		major_version(1),
		minor_version(1),
		request_id(0),
		trace_bit(false)
	{
	}

	int major_version;
	int minor_version;
	uint64_t request_id;
	bool trace_bit;
	std::string remote_endpoint;
	std::string local_endpoint;
};

http_request::http_request() : m_data(new http_request_data)
{
}

http_request::http_request(const boost::none_t &none) : swarm::http_request(none)
{
}

http_request::http_request(http_request &&other) :
	swarm::http_request(std::move(other))
{
	std::swap(m_data, other.m_data);
}

http_request::http_request(const http_request &other) :
	swarm::http_request(other),
	m_data(new http_request_data(*other.m_data))
{
}

http_request::~http_request()
{
}

http_request &http_request::operator =(http_request &&other)
{
	std::swap(m_data, other.m_data);
	swarm::http_request::operator=(std::move(other));
	return *this;
}

http_request &http_request::operator =(const http_request &other)
{
	http_request tmp(other);
	std::swap(m_data, tmp.m_data);
	swarm::http_request::operator=(tmp);
	return *this;
}

uint64_t http_request::request_id() const
{
	return m_data->request_id;
}

void http_request::set_request_id(uint64_t request_id)
{
	m_data->request_id = request_id;
}

bool http_request::trace_bit() const
{
	return m_data->trace_bit;
}

void http_request::set_trace_bit(bool trace_bit)
{
	m_data->trace_bit = trace_bit;
}

const std::string &http_request::remote_endpoint() const
{
	return m_data->remote_endpoint;
}

void http_request::set_remote_endpoint(const std::string &remote_endpoint)
{
	m_data->remote_endpoint = remote_endpoint;
}

const std::string &http_request::local_endpoint() const
{
	return m_data->local_endpoint;
}

void http_request::set_local_endpoint(const std::string &local_endpoint)
{
	m_data->local_endpoint = local_endpoint;
}

void http_request::set_http_version(int major_version, int minor_version)
{
	m_data->major_version = major_version;
	m_data->minor_version = minor_version;
}

int http_request::http_major_version() const
{
	return m_data->major_version;
}

int http_request::http_minor_version() const
{
	return m_data->minor_version;
}

bool http_request::is_keep_alive() const
{
	if (auto keep_alive = headers().is_keep_alive()) {
		return *keep_alive;
	}

	return http_major_version() == 1 && http_minor_version() >= 1;
}

} // namespace thevoid
} // namespace ioremap
