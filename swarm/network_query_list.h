#ifndef IOREMAP_SWARM_NETWORK_QUERY_LIST_H
#define IOREMAP_SWARM_NETWORK_QUERY_LIST_H

#include <memory>
#include <utility>
#include <string>

#include <boost/optional.hpp>

namespace ioremap {
namespace swarm {

class network_query_list_private;

class network_query_list
{
public:
	network_query_list();
	network_query_list(const std::string &query);

	~network_query_list();

	void set_query(const std::string &query);
	std::string to_string() const;

	size_t count() const;
	std::pair<std::string, std::string> item(size_t index) const;
	void add_item(const std::string &key, const std::string &value);
	void remove_item(size_t index);

	bool has_item(const std::string &key) const;
	std::string item_value(const std::string &key) const;
	boost::optional<std::string> try_item(const std::string &key) const;
	boost::optional<std::string> try_item(const char *key) const;

private:
	network_query_list(const network_query_list &other);
	network_query_list &operator =(const network_query_list &other);

	std::unique_ptr<network_query_list_private> p;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_NETWORK_QUERY_LIST_H
