/*
 * 2013+ Copyright (c) Ruslan Nigatullin <euroelessar@yandex.ru>
 * 2013+ Copyright (c) Evgeniy Polyakov <zbr@ioremap.net>
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

#ifndef __IOREMAP_THEVOID_ELLIPTICS_JSONVALUE_HPP
#define __IOREMAP_THEVOID_ELLIPTICS_JSONVALUE_HPP

#include <boost/asio.hpp>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"

#include <thevoid/rapidjson/stringbuffer.h>
#include <thevoid/rapidjson/prettywriter.h>
#pragma GCC diagnostic pop

namespace ioremap { namespace thevoid { namespace elliptics {

class JsonValue : public rapidjson::Value
{
public:
	JsonValue() {
		SetObject();
	}

	~JsonValue() {
	}

	std::string ToString() {
		rapidjson::StringBuffer buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);

		Accept(writer);
		buffer.Put('\n');

		return std::string(buffer.GetString(), buffer.GetSize());
	}

	rapidjson::MemoryPoolAllocator<> &GetAllocator() {
		return m_allocator;
	}

private:
	rapidjson::MemoryPoolAllocator<> m_allocator;
};

}}}

#endif /* __IOREMAP_THEVOID_ELLIPTICS_JSONVALUE_HPP */
