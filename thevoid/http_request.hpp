#ifndef IOREMAP_THEVOID_HTTP_REQUEST_H
#define IOREMAP_THEVOID_HTTP_REQUEST_H

#include <swarm/http_request.hpp>

namespace ioremap {
namespace thevoid {

class http_request : public swarm::http_request
{
public:
	http_request();
	http_request(const boost::none_t &);
	http_request(http_request &&other);
	http_request(const http_request &other);
	~http_request();

	http_request &operator =(http_request &&other);
	http_request &operator =(const http_request &other);

	uint64_t request_id() const;
	void set_request_id(uint64_t request_id);

	bool trace_bit() const;
	void set_trace_bit(bool trace_bit);

	const std::string &remote_endpoint() const;
	void set_remote_endpoint(const std::string &remote_endpoint);

	const std::string &local_endpoint() const;
	void set_local_endpoint(const std::string &local_endpoint);

	void set_http_version(int major_version, int minor_version);
	int http_major_version() const;
	int http_minor_version() const;

	// Checks by Connection header and HTTP version if connection is Keep-Alive
	bool is_keep_alive() const;
};

} // namespace thevoid
} // namespace ioremap

#endif // IOREMAP_THEVOID_HTTP_REQUEST_H
