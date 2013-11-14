#include "elliptics_auth.hpp"

namespace ioremap {
namespace thevoid {
namespace elliptics {

elliptics_auth::elliptics_auth()
{
}

bool elliptics_auth::initialize(const rapidjson::Value &config, const ioremap::elliptics::node &node, const swarm::logger &logger)
{
	(void) node;
	m_logger = logger;

	if (!config.HasMember("auth")) {
		m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.auth\" field is missed");
		return false;
	}

	const rapidjson::Value &auth = config["auth"];

	if (!auth.IsArray()) {
		m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.auth\" is not array");
		return false;
	}

	for (size_t i = 0; i < auth.Size(); ++i) {
		const rapidjson::Value &value = auth[i];
		if (!value.IsObject()) {
			m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.auth[%zu]\" is not object", i);
			return false;
		}

		auto ns = value.FindMember("namespace");
		auto key = value.FindMember("key");
		if (!ns || !ns->value.IsString()) {
			m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.auth[%zu].namespace\" field is not string", i);
			return false;
		}
		if (!key || !key->value.IsString()) {
			m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.auth[%zu].key\" field is not string", i);
			return false;
		}

		m_keys[ns->value.GetString()] = key->value.GetString();
	}

	return true;
}

bool elliptics_auth::check(const swarm::http_request &request)
{
	if (auto ns = request.url().query().item_value("namespace")) {
		if (auto auth = request.headers().get("Authentication")) {
			auto it = m_keys.find(*ns);
			return it != m_keys.end() && it->second == *auth;
		}
	}

	return false;
}

} // namespace elliptics
} // namespace thevoid
} // namespace ioremap
