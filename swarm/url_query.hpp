#ifndef IOREMAP_SWARM_NETWORK_QUERY_LIST_H
#define IOREMAP_SWARM_NETWORK_QUERY_LIST_H

#include <memory>
#include <utility>
#include <string>

#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>

namespace ioremap {
namespace swarm {

class network_query_list_private;

class url_query
{
public:
	url_query();
	url_query(url_query &&other);
	url_query(const url_query &other);
	url_query(const std::string &query);

	~url_query();

	url_query &operator =(url_query &&other);
	url_query &operator =(const url_query &other);

	void set_query(const std::string &query);
	std::string to_string() const;

	size_t count() const;
	const std::pair<std::string, std::string> &item(size_t index) const;
	void add_item(const std::string &key, const std::string &value);
	void remove_item(size_t index);

	bool has_item(const std::string &key) const;
	boost::optional<std::string> item_value(const std::string &key) const;
	boost::optional<std::string> item_value(const char *key) const;

	template <typename T>
	T item_value(const std::string &key, const T &default_value) const
	{
		if (auto value = item_value(key))
			return boost::lexical_cast<T>(value);
		return default_value;
	}

	template <typename T>
	T item_value(const char *key, const T &default_value) const
	{
		if (auto value = item_value(key))
			return boost::lexical_cast<T>(value);
		return default_value;
	}

private:
	std::unique_ptr<network_query_list_private> p;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_NETWORK_QUERY_LIST_H
