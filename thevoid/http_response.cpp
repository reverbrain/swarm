#include "http_response.hpp"
#include "../swarm/http_response_p.hpp"

namespace ioremap {
namespace thevoid {

class http_response_data : public swarm::http_response_data
{
public:
};

#define M_DATA() static_cast<http_response_data *>(m_data.get())

http_response::http_response() : swarm::http_response(*new http_response_data)
{
}

http_response::http_response(const boost::none_t &none) : http_response(none)
{
}

http_response::http_response(http_response &&other) :
	swarm::http_response(std::move(other))
{
}

http_response::http_response(const http_response &other) :
	swarm::http_response(*new http_response_data(*static_cast<http_response_data *>(other.m_data.get())))
{
}

http_response::~http_response()
{
}

http_response &http_response::operator =(http_response &&other)
{
	using std::swap;
	http_response tmp(other);
	swap(m_data, tmp.m_data);
	return *this;
}

http_response &http_response::operator =(const http_response &other)
{
	using std::swap;
	http_response tmp(other);
	swap(m_data, tmp.m_data);
	return *this;
}

} // namespace thevoid
} // namespace ioremap
