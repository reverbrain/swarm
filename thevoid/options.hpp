#ifndef IOREMAP_THEVOID_OPTIONS_HPP
#define IOREMAP_THEVOID_OPTIONS_HPP

#include "http_request.hpp"

namespace ioremap {
namespace thevoid {
namespace detail {
namespace options {

template <typename T>
struct option_base : public T
{
	template <typename... Args>
	option_base(Args ...args) : T(std::forward<Args>(args)...)
	{
	}
};

struct option_true
{
	bool operator() (const http_request &request) const
	{
		(void) request;
		return true;
	}
};

struct option_exact_match
{
	std::string match;

	option_exact_match(std::string match) : match(std::move(match))
	{
	}

	bool operator() (const http_request &request) const
	{
		return match == request.url().path();
	}
};

struct option_prefix_match
{
	std::string match;

	option_prefix_match(std::string match) : match(std::move(match))
	{
	}

	bool operator() (const http_request &request) const
	{
		return request.url().path().compare(0, match.size(), match) != 0;
	}
};

struct option_method
{
	std::string method;

	option_method(std::string method) : method(std::move(method))
	{
	}

	bool operator() (const http_request &request) const
	{
		return request.method() == method;
	}
};

template <typename First, typename Second>
struct option_or
{
	First first;
	Second second;

	option_or(First &&first, Second &&second) : first(std::move(first)), second(std::move(second))
	{
	}

	bool operator() (const http_request &request) const
	{
		return first(request) || second(request);
	}
};

template <typename First, typename Second>
struct option_and
{
	First first;
	Second second;

	option_and(First &&first, Second &&second) : first(std::move(first)), second(std::move(second))
	{
	}

	bool operator() (const http_request &request) const
	{
		return first(request) && second(request);
	}
};

template <typename First, typename Second>
inline option_base<option_or<First, Second>> operator ||(option_base<First> &&first, option_base<Second> &&second)
{
	return option_base<option_or<First, Second>>(
		std::move(static_cast<First &&>(first)),
		std::move(static_cast<Second &&>(second))
	);
}

template <typename First, typename Second>
inline option_base<option_and<First, Second>> operator &&(option_base<First> &&first, option_base<Second> &&second)
{
	return option_base<option_and<First, Second>>(
		std::move(static_cast<First &&>(first)),
		std::move(static_cast<Second &&>(second))
	);
}

} } // namespace detail::options

namespace options {

static inline detail::options::option_base<detail::options::option_exact_match> exact_match(std::string match)
{
	return detail::options::option_base<detail::options::option_exact_match>(std::move(match));
}

static inline detail::options::option_base<detail::options::option_prefix_match> prefix_match(std::string match)
{
	return detail::options::option_base<detail::options::option_prefix_match>(std::move(match));
}

static inline detail::options::option_base<detail::options::option_method> method(std::string match)
{
	return detail::options::option_base<detail::options::option_method>(std::move(match));
}

} } } // namespace ioremap::thevoid::options

#endif // IOREMAP_THEVOID_OPTIONS_HPP
