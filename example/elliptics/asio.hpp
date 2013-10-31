/*
 * 2013+ Copyright (c) Ruslan Nigatullin <euroelessar@yandex.ru>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __IOREMAP_THEVOID_ELLIPTICS_ASIO_HPP
#define __IOREMAP_THEVOID_ELLIPTICS_ASIO_HPP

#include <boost/asio.hpp>
#include <elliptics/session.hpp>

// must be the first header, since thevoid internally uses X->boost::buffer conversion,
// which must be present at compile time

namespace boost { namespace asio {

inline const_buffer buffer(const ioremap::elliptics::data_pointer &data)
{
	return buffer(data.data(), data.size());
}

}}

#endif /* __IOREMAP_THEVOID_ELLIPTICS_ASIO_HPP */
