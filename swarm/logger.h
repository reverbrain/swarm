#ifndef IOREMAP_SWARM_LOGGER_H
#define IOREMAP_SWARM_LOGGER_H

#include <memory>

namespace ioremap {
namespace swarm {

class logger_interface
{
public:
	virtual ~logger_interface() {}
	virtual void log(int level, const char *msg) = 0;
};

enum log_level {
	LOG_DATA = 0,
	LOG_ERROR = 1,
	LOG_INFO = 2,
	LOG_NOTICE = 3,
	LOG_DEBUG = 4
};

class logger_data;

class logger
{
public:
	logger();
	logger(logger_interface *impl, int level);
	logger(const char *file, int level);
	~logger();

	int get_level() const;
	void set_level(int level);

	void log(int level, const char *format, ...) __attribute__ ((format(printf, 3, 4)));

private:
	std::shared_ptr<logger_data> m_data;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_LOGGER_H
