#ifndef IOREMAP_THEVOID_ELLIPTICS_ELLIPTICS_AUTH_HPP
#define IOREMAP_THEVOID_ELLIPTICS_ELLIPTICS_AUTH_HPP

#include <thevoid/server.hpp>
#include <swarm/http_request.hpp>
#include <swarm/logger.hpp>
#include <elliptics/session.hpp>

namespace ioremap {
namespace thevoid {

class auth_interface
{
public:
	virtual ~auth_interface() {}
	virtual bool check(const swarm::http_request &request) = 0;
};

namespace elliptics {

class elliptics_auth : public auth_interface
{
public:
	elliptics_auth();

	bool initialize(const rapidjson::Value &config, const ioremap::elliptics::node &node, const swarm::logger &logger);
	bool check(const swarm::http_request &request);

private:
	swarm::logger m_logger;
	std::map<std::string, std::string> m_keys;
};

} // namespace elliptics
} // namespace thevoid
} // namespace ioremap

#endif // IOREMAP_THEVOID_ELLIPTICS_ELLIPTICS_AUTH_HPP
