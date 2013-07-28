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

namespace {
	static inline std::string lexical_cast(size_t value) {
		if (value == 0) {
			return std::string("0");
		}

		std::string result;
		size_t length = 0;
		size_t calculated = value;
		while (calculated) {
			calculated /= 10;
			++length;
		}

		result.resize(length);
		while (value) {
			--length;
			result[length] = '0' + (value % 10);
			value /= 10;
		}

		return result;
	}
}

namespace ioremap { namespace thevoid { namespace elliptics {

class JsonValue : public rapidjson::Value
{
public:
	JsonValue() {
		SetObject();
	}

	~JsonValue() {
	}

	static void set_time(rapidjson::Value &obj, rapidjson::Document::AllocatorType &alloc, long tsec, long usec) {
		char str[64];
		struct tm tm;

		localtime_r((time_t *)&tsec, &tm);
		strftime(str, sizeof(str), "%F %Z %R:%S", &tm);

		char time_str[128];
		snprintf(time_str, sizeof(time_str), "%s.%06lu", str, usec);

		obj.SetObject();

		rapidjson::Value tobj(time_str, strlen(time_str), alloc);
		obj.AddMember("time", tobj, alloc);

		std::string raw_time = lexical_cast(tsec) + "." + lexical_cast(usec);
		rapidjson::Value tobj_raw(raw_time.c_str(), raw_time.size(), alloc);
		obj.AddMember("time-raw", tobj_raw, alloc);
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
