#include "network_query_list.h"

#include <uriparser/Uri.h>

#include <vector>
#include <string>

namespace ioremap {
namespace swarm {

class network_query_list_private
{
public:
	std::vector<std::pair<std::string, std::string> > items;
};

url_query::url_query() : p(new network_query_list_private)
{
}

url_query::url_query(url_query &&other) : p(new network_query_list_private)
{
	using std::swap;
	swap(p, other.p);
}

url_query::url_query(const url_query &other) : p(new network_query_list_private(*other.p))
{
}

url_query::url_query(const std::string &query) : p(new network_query_list_private)
{
	set_query(query);
}

url_query::~url_query()
{
}

url_query &url_query::operator =(url_query &&other)
{
	using std::swap;
	url_query tmp;
	swap(p, tmp.p);
	swap(p, other.p);
	return *this;
}

url_query &url_query::operator =(const url_query &other)
{
	using std::swap;
	url_query tmp(other);
	swap(p, tmp.p);
	return *this;
}

void url_query::set_query(const std::string &query)
{
	int item_count = 0;
	UriQueryListA *query_list = NULL;

	uriDissectQueryMallocA(&query_list, &item_count,
			       query.c_str(), query.c_str() + query.size());

	p->items.clear();
	p->items.reserve(item_count);

	for (auto it = query_list; it; it = it->next) {
		p->items.emplace_back(it->key, it->value ? it->value : std::string());
	}

	uriFreeQueryListA(query_list);
}

std::string url_query::to_string() const
{
	if (p->items.empty())
		return std::string();

	std::vector<UriQueryListA> items(p->items.size());

	for (size_t i = 0; i < p->items.size(); ++i) {
		auto &list_entry = items[i];
		auto &item = p->items[i];

		list_entry.key = item.first.c_str();
		list_entry.value = item.second.c_str();

		if (i + 1 < p->items.size())
			list_entry.next = &items[i + 1];
		else
			list_entry.next = NULL;
	}

	int requiredSize = 0;
	uriComposeQueryCharsRequiredA(&items[0], &requiredSize);

	std::string result;
	result.resize(requiredSize);
	uriComposeQueryA(&result[0], &items[0], result.size(), &requiredSize);
	result.resize(requiredSize);

	return result;
}

size_t url_query::count() const
{
	return p->items.size();
}

std::pair<std::string, std::string> url_query::item(size_t index) const
{
	return p->items[index];
}

void url_query::add_item(const std::string &key, const std::string &value)
{
	p->items.emplace_back(key, value);
}

void url_query::remove_item(size_t index)
{
	p->items.erase(p->items.begin() + index);
}

bool url_query::has_item(const std::string &key) const
{
	for (size_t i = 0; i < p->items.size(); ++i) {
		if (p->items[i].first == key)
			return true;
	}
	return false;
}

boost::optional<std::string> url_query::item_value(const std::string &key) const
{
	for (size_t i = 0; i < p->items.size(); ++i) {
		if (p->items[i].first == key)
			return p->items[i].second;
	}
	return boost::none;
}

boost::optional<std::string> url_query::item_value(const char *key) const
{
	const size_t key_size = strlen(key);

	for (size_t i = 0; i < p->items.size(); ++i) {
		const auto &item = p->items[i];
		if (item.first.compare(0, item.first.size(), key, key_size) == 0)
			return item.second;
	}
	return boost::none;
}

} // namespace swarm
} // namespace ioremap
