#include "http_response.hpp"

#include <algorithm>

namespace ioremap {
namespace thevoid {

class http_response_data
{
};

http_response::http_response() : m_data(new http_response_data)
{
}

http_response::http_response(const boost::none_t &none) : swarm::http_response(none)
{
}

http_response::http_response(http_response &&other) :
	swarm::http_response(std::move(other))
{
	std::swap(m_data, other.m_data);
}

http_response::http_response(const http_response &other) :
	swarm::http_response(other),
	m_data(new http_response_data(*other.m_data))
{
}

http_response::~http_response()
{
}

http_response &http_response::operator =(http_response &&other)
{
	std::swap(m_data, other.m_data);
	swarm::http_response::operator=(std::move(other));
	return *this;
}

http_response &http_response::operator =(const http_response &other)
{
	http_response tmp(other);
	std::swap(m_data, tmp.m_data);
	swarm::http_response::operator=(tmp);
	return *this;
}

} // namespace thevoid
} // namespace ioremap
