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
		if (!ns.IsString()) {
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

static std::string to_lower(const std::string &str)
{
	std::string result;
	result.resize(str.size());
	std::transform(str.begin(), str.end(), result.begin(), tolower);
	return result;
}

static void check_hash(const swarm::logger &logger, const std::string &name, const std::string &message)
{
	dnet_raw_id signature;
	char signature_str[DNET_ID_SIZE * 2 + 1];

	dnet_digest_transform_raw(message.c_str(), message.size(), signature.id, DNET_ID_SIZE);
	dnet_dump_id_len_raw(signature.id, DNET_ID_SIZE, signature_str);

	logger.log(swarm::SWARM_LOG_DATA, "name: \"%s\", result: \"%s\"", name.c_str(), signature_str);
}

std::string elliptics_auth::generate_signature(const swarm::http_request &request, const std::string &key) const
{
	const auto &url = request.url();
	const auto &query = url.query();
	const auto &original_headers = request.headers().all();

	std::vector<swarm::headers_entry> headers;
	for (auto it = original_headers.begin(); it != original_headers.end(); ++it) {
		std::string name = to_lower(it->first);
		if (name.compare(0, 6, "x-ell-") == 0) {
			headers.emplace_back(std::move(name), it->second);
		}
	}

	std::sort(headers.begin(), headers.end());

	std::vector<std::pair<std::string, std::string> > query_items;

	for (size_t i = 0; i < query.count(); ++i) {
		const auto &item = query.item(i);
		query_items.emplace_back(to_lower(item.first), item.second);
	}

	std::sort(query_items.begin(), query_items.end());

	swarm::url result_url;
	result_url.set_path(url.path());

	for (auto it = query_items.begin(); it != query_items.end(); ++it) {
		result_url.query().add_item(it->first, it->second);
	}

	std::string text = request.method();
	text += '\n';
	text += result_url.to_string();
	text += '\n';

	for (auto it = headers.begin(); it != headers.end(); ++it) {
		text += it->first;
		text += ':';
		text += it->second;
		text += '\n';
	}

	check_hash(m_logger, "key", key);
	check_hash(m_logger, "message", text);

	dnet_raw_id signature;
	char signature_str[DNET_ID_SIZE * 2 + 1];

	dnet_digest_auth_transform_raw(text.c_str(), text.size(), key.c_str(), key.size(), signature.id, DNET_ID_SIZE);
	dnet_dump_id_len_raw(signature.id, DNET_ID_SIZE, signature_str);

	m_logger.log(swarm::SWARM_LOG_DATA, "signature: \"%s\", result: \"%s\"", text.c_str(), signature_str);

	return signature_str;
}

} // namespace elliptics
} // namespace thevoid
} // namespace ioremap
