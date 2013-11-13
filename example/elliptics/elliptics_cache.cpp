#include "elliptics_cache.hpp"

#include <msgpack.hpp>

namespace msgpack
{
using namespace ioremap::elliptics;

inline dnet_raw_id &operator >>(msgpack::object o, dnet_raw_id &v)
{
	if (o.type != msgpack::type::RAW || o.via.raw.size != sizeof(v.id)) {
		throw msgpack::type_error();
	}
	memcpy(v.id, o.via.raw.ptr, sizeof(v.id));
	return v;
}

template <typename Stream>
inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const dnet_raw_id &v)
{
	o.pack_raw(sizeof(v.id));
	o.pack_raw_body(reinterpret_cast<const char *>(v.id), sizeof(v.id));
	return o;
}

template <typename K, typename V, typename H, typename E>
inline std::unordered_map<K, V, H, E> &operator >>(msgpack::object o, std::unordered_map<K, V, H, E> &v)
{
	if (o.type != type::MAP) {
		throw type_error();
	}
	object_kv *pointer = o.via.map.ptr;
	object_kv * const pointer_end = o.via.map.ptr + o.via.map.size;
	for(; pointer != pointer_end; ++pointer) {
		K key;
		pointer->key.convert(&key);
		pointer->val.convert(&v[key]);
	}
	return v;
}

template <typename Stream, typename K, typename V, typename H, typename E>
inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const std::unordered_map<K, V, H, E> &v)
{
	o.pack_map(v.size());
	for(auto it = v.begin(), it_end = v.end(); it != it_end; ++it) {
		o.pack(it->first);
		o.pack(it->second);
	}
	return o;
}
}

namespace ioremap {
namespace thevoid {
namespace elliptics {

elliptics_cache::elliptics_cache()
{
}

bool elliptics_cache::initialize(const rapidjson::Value &application_config, const ioremap::elliptics::node &node, const swarm::logger &logger)
{
	m_logger = logger;

	if (!application_config.HasMember("cache")) {
		m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.cache\" field is missed");
		return false;
	}

	const rapidjson::Value &config = application_config["cache"];

	if (!config.HasMember("groups")) {
		m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.cache.groups\" field is missed");
		return false;
	}

	if (!config.HasMember("name")) {
		m_logger.log(swarm::SWARM_LOG_ERROR, "\"application.cache.groups\" field is missed");
		return false;
	}

	m_key = std::string(config["name"].GetString());
	m_timeout = 30;
	if (auto tmp = config.FindMember("timeout")) {
		m_timeout = tmp->value.GetInt();
	}

	auto &groupsArray = config["groups"];
	std::transform(groupsArray.Begin(), groupsArray.End(),
		std::back_inserter(m_groups),
		std::bind(&rapidjson::Value::GetInt, std::placeholders::_1));

	m_session.reset(new ioremap::elliptics::session(node));
	m_session->set_groups(m_groups);

	m_thread = boost::thread(std::bind(&elliptics_cache::sync_thread, shared_from_this()));

	return true;
}

void elliptics_cache::stop()
{
	m_need_exit = true;
	m_thread.join();
}

std::vector<int> elliptics_cache::groups(const ioremap::elliptics::key &key)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_cache_groups.find(key.raw_id());
	if (it != m_cache_groups.end()) {
		return it->second;
	}

	return std::vector<int>();
}

void elliptics_cache::sync_thread()
{
	while (!m_need_exit) {
		ioremap::elliptics::session session = m_session->clone();
		session.read_data(m_key, 0, 0).connect(std::bind(
			&elliptics_cache::on_read_finished, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
        
        int timeout = m_timeout;
		while (!m_need_exit && timeout-- > 0) {
			sleep(1);
		}
	}
}

void elliptics_cache::on_read_finished(const ioremap::elliptics::sync_read_result &result, const ioremap::elliptics::error_info &error)
{
	if (error) {
		m_logger.log(swarm::SWARM_LOG_ERROR, "Failed to access groups file: %s", error.message().c_str());
		return;
	}

	const ioremap::elliptics::read_result_entry &entry = result[0];
	auto file = entry.file();

	unordered_map cache_groups;

	msgpack::unpacked msg;
	msgpack::unpack(&msg, file.data<char>(), file.size());
	msg.get().convert(&cache_groups);

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		using std::swap;
		swap(m_cache_groups, cache_groups);
	}
}

size_t elliptics_cache::hash_impl::operator()(const dnet_raw_id &key) const
{
	// Take last sizeof(size_t) bytes as indexes use first 256
	const uint8_t *id = key.id;
	id += (DNET_ID_SIZE - sizeof(size_t));
	return *reinterpret_cast<const size_t *>(id);
}

bool elliptics_cache::equal_impl::operator() (const dnet_raw_id &first, const dnet_raw_id &second) const
{
	return memcmp(first.id, second.id, DNET_ID_SIZE) == 0;
}

} // namespace elliptics
} // namespace thevoid
} // namespace ioremap
