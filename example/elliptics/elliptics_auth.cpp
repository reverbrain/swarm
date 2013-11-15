#include "elliptics_auth.hpp"

namespace ioremap {
namespace thevoid {

simple_password_auth::simple_password_auth()
{
}

bool simple_password_auth::initialize(const rapidjson::Value &config, const swarm::logger &logger)
{
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

		if (!value.HasMember("namespace")) {
			m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.auth[%zu].namespace\" field is missed", i);
			return false;
		}

		if (!value.HasMember("key")) {
			m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.auth[%zu].key\" field is missed", i);
			return false;
		}

		auto &ns = value["namespace"];
		auto &key = value["key"];
		if (ns.IsString()) {
			m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.auth[%zu].namespace\" field is not string", i);
			return false;
		}
		if (!key.IsString()) {
			m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.auth[%zu].key\" field is not string", i);
			return false;
		}

		m_keys[ns.GetString()] = key.GetString();
	}

	return true;
}

bool simple_password_auth::check(const swarm::http_request &request)
{
	if (auto ns = request.url().query().item_value("namespace")) {
		if (auto auth = request.headers().get("Authorization")) {
			auto it = m_keys.find(*ns);
			return it != m_keys.end() && it->second == *auth;
		}
	}

	return false;
}

namespace elliptics {

elliptics_auth::elliptics_auth()
{
}

bool elliptics_auth::initialize(const rapidjson::Value &config, const ioremap::elliptics::node &node, const swarm::logger &logger)
{
	m_node.reset(new ioremap::elliptics::node(node));
	return simple_password_auth::initialize(config, logger);
}

bool elliptics_auth::check(const swarm::http_request &request)
{
	if (auto ns = request.url().query().item_value("namespace")) {
		auto it = m_keys.find(*ns);
		if (it == m_keys.end()) {
			return false;
		}

		if (auto auth = request.headers().get("Authorization")) {
			auto key = generate_signature(request, it->second);

			return *auth == key;
		}
	}

	return false;
}

std::string elliptics_auth::generate_signature(const swarm::http_request &request, const std::string &key) const
{
	const auto &url = request.url();
	auto headers_copy = request.headers();
	headers_copy.remove("Authorization");

	std::vector<swarm::headers_entry> &headers = headers_copy.all();
	for (auto it = headers.begin(); it != headers.end(); ++it) {
		std::transform(it->first.begin(), it->first.end(), it->first.begin(), tolower);
	}

	std::sort(headers.begin(), headers.end());

	std::string text = url.path();
	text += '\n';
	text += url.raw_query();
	text += '\n';

	for (auto it = headers.begin(); it != headers.end(); ++it) {
		text += it->first;
		text += ':';
		text += it->second;
		text += '\n';
	}
	text += key;
	text += '\n';

	dnet_raw_id signature;
	char signature_str[DNET_ID_SIZE * 2 + 1];

	dnet_transform_node(m_node->get_native(), text.c_str(), text.size(), signature.id, sizeof(signature.id));

	return dnet_dump_id_len_raw(signature.id, DNET_ID_SIZE, signature_str);
}

} // namespace elliptics
} // namespace thevoid
} // namespace ioremap
