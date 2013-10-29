#include "http_response.hpp"
#include "http_request.hpp"

namespace ioremap {
namespace swarm {

class network_reply_data
{
public:
	network_reply_data()
		: error(0), code(0)
	{
	}
	network_reply_data(const network_reply_data &o) = default;

	int error;

	int code;
	boost::optional<std::string> reason;
	http_headers headers;
};

http_response::http_response() : m_data(new network_reply_data)
{
}

http_response::http_response(const boost::none_t &)
{
}

http_response::http_response(http_response &&other)
{
	using std::swap;
	swap(m_data, other.m_data);
}

http_response::http_response(const http_response &other) : m_data(new network_reply_data(*other.m_data))
{
}

http_response::~http_response()
{
}

http_response &http_response::operator =(http_response &&other)
{
	using std::swap;
	swap(m_data, other.m_data);
	return *this;
}

http_response &http_response::operator =(const http_response &other)
{
	using std::swap;
	http_response tmp(other);
	swap(m_data, tmp.m_data);
	return *this;
}

int http_response::code() const
{
	return m_data->code;
}

void http_response::set_code(int code)
{
	m_data->code = code;
}

boost::optional<std::string> http_response::reason() const
{
	return m_data->reason;
}

void http_response::set_reason(const std::string &reason)
{
	m_data->reason = reason;
}

int http_response::error() const
{
	return m_data->error;
}

void http_response::set_error(int error)
{
	m_data->error = error;
}

http_headers &http_response::headers()
{
	return m_data->headers;
}

const http_headers &http_response::headers() const
{
	return m_data->headers;
}

void http_response::set_headers(const http_headers &headers)
{
	m_data->headers = headers;
}

void http_response::set_headers(http_headers &&headers)
{
	m_data->headers = headers;
}

} // namespace swarm
} // namespace ioremap
