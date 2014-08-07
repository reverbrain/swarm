#ifndef IOREMAP_THEVOID_HTTP_RESPONSE_H
#define IOREMAP_THEVOID_HTTP_RESPONSE_H

#include <swarm/http_response.hpp>

namespace ioremap {
namespace thevoid {

class http_response : public swarm::http_response
{
public:
	http_response();
	http_response(const boost::none_t &);
	http_response(http_response &&other);
	http_response(const http_response &other);
	~http_response();

	http_response &operator =(http_response &&other);
	http_response &operator =(const http_response &other);
};

} // namespace thevoid
} // namespace ioremap

#endif // IOREMAP_THEVOID_HTTP_RESPONSE_H
