#ifndef IOREMAP_SWARM_HTTP_HEADERS_HPP
#define IOREMAP_SWARM_HTTP_HEADERS_HPP

#include <boost/optional.hpp>
#include <vector>
#include <string>
#include <memory>

namespace ioremap {
namespace swarm {

class http_headers_private;

typedef std::pair<std::string, std::string> headers_entry;

class http_headers
{
public:
	http_headers();
	http_headers(http_headers &&other);
	http_headers(const http_headers &other);
	~http_headers();

	http_headers &operator =(http_headers &&other);
	http_headers &operator =(const http_headers &other);

	// List of headers
	const std::vector<headers_entry> &all() const;
	bool has(const std::string &name) const;
	boost::optional<std::string> get(const std::string &name) const;
	boost::optional<std::string> get(const char *name) const;
	void set(const std::vector<headers_entry> &headers);
	void set(const headers_entry &header);
	void set(const std::string &name, const std::string &value);
	void add(const headers_entry &header);
	void add(const std::string &name, const std::string &value);

	// Last-Modified, UTC
	bool has_last_modified() const;
	time_t last_modified() const;
	boost::optional<std::string> last_modified_string() const;
	void set_last_modified(const std::string &last_modified);
	void set_last_modified(time_t last_modified);

	// If-Modified-Since, UTC
	bool has_if_modified_since() const;
	time_t if_modified_since() const;
	boost::optional<std::string> if_modified_since_string() const;
	void set_if_modified_since(const std::string &time);
	void set_if_modified_since(time_t time);

	// Content length
	void set_content_length(size_t length);
	bool has_content_length() const;
	boost::optional<size_t> content_length() const;

	// Content type
	void set_content_type(const std::string &type);
	bool has_content_type() const;
	boost::optional<std::string> content_type() const;

private:
	std::unique_ptr<http_headers_private> p;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_HTTP_HEADERS_HPP
