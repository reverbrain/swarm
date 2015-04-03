#ifndef IOREMAP_THEVOID_SIGNAL_HANDLER_P_HPP
#define IOREMAP_THEVOID_SIGNAL_HANDLER_P_HPP

#include <memory>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <blackhole/utils/atomic.hpp>

#include "server.hpp"

namespace ioremap { namespace thevoid {

struct signal_handler {
	typedef boost::asio::deadline_timer timer;
	typedef std::shared_ptr<timer> timer_ptr;

	signal_handler(
			base_server* server,
			const boost::posix_time::time_duration &timeout = boost::posix_time::seconds(1)
		);

	void run(boost::asio::io_service* io_service);

	void handle_timer(const boost::system::error_code &error);

	// received stop signal
	static std::atomic<int> stop_request;

	// received reload signal and its global revision number
	static std::atomic<int> reload_request;
	static std::atomic<size_t> reload_rev;

	base_server* m_server;
	timer_ptr m_timer;
	boost::posix_time::time_duration m_timeout;

	size_t m_reload_signal_rev;
};

}} // namespace ioremap::thevoid

#endif // IOREMAP_THEVOID_SIGNAL_HANDLER_P_HPP
