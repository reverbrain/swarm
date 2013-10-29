#ifndef IOREMAP_SWARM_HTTP_REQUEST_HPP
#define IOREMAP_SWARM_HTTP_REQUEST_HPP

#include "url.hpp"
#include "http_headers.hpp"

namespace ioremap {
namespace swarm {

class network_request_data;

class http_request
{
public:
	http_request();
	http_request(const boost::none_t &);
	http_request(http_request &&other);
	http_request(const http_request &other);
	~http_request();

	http_request &operator =(http_request &&other);
	http_request &operator =(const http_request &other);

	// Request URL
	const swarm::url &url() const;
	void set_url(const swarm::url &url);
	void set_url(const std::string &url);

	// Checks by Connection header and HTTP version if connection is Keep-Alive
	bool is_keep_alive() const;

	// HTTP headers
	http_headers &headers();
	const http_headers &headers() const;

	// TheVoid specific arguments
	void set_http_version(int major_version, int minor_version);
	int http_major_version() const;
	int http_minor_version() const;

	void set_method(const std::string &method);
	std::string method() const;

private:
	std::unique_ptr<network_request_data> m_data;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_HTTP_REQUEST_HPP
