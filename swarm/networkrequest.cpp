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

#include "networkrequest.h"
#if __GNUC__ == 4 && __GNUC_MINOR__ < 5
#  include <cstdatomic>
#else
#  include <atomic>
#endif
#include <cstring>
#include <algorithm>

namespace ioremap {
namespace swarm {

#define HTTP_DATE_RFC_1123 "%a, %d %b %Y %H:%M:%S %Z" // Sun, 06 Nov 1994 08:49:37 GMT
#define HTTP_DATE_RFC_850  "%A, %d-%b-%y %H:%M:%S %Z" // Sunday, 06-Nov-94 08:49:37 GMT
#define HTTP_DATE_ASCTIME  "%a %b %e %H:%M:%S %Y"     // Sun Nov  6 08:49:37 1994

#define LAST_MODIFIED_HEADER "Last-Modified"
#define IF_MODIFIED_SINCE_HEADER "If-Modified-Since"
#define CONNECTION_HEADER "Connection"
#define CONNECTION_HEADER_KEEP_ALIVE "Keep-Alive"
#define CONTENT_LENGTH_HEADER "Content-Length"
#define CONTENT_TYPE_HEADER "Content-Type"

static bool are_case_insensitive_equal(const std::string &first, const char *second, const size_t second_size)
{
    if (first.size() != second_size)
        return false;

    for (size_t i = 0; i < second_size; ++i) {
        if (toupper(first[i]) != toupper(second[i]))
            return false;
    }

    return true;
}

static bool are_case_insensitive_equal(const std::string &first, const std::string &second)
{
    return are_case_insensitive_equal(first, second.c_str(), second.size());
}

static std::string convert_to_http_date(time_t time)
{
    struct tm time_data;
    struct tm *tm_result = gmtime_r(&time, &time_data);

    if (!tm_result)
        return std::string();

    char buffer[1024];
    strftime(buffer, sizeof(buffer), HTTP_DATE_RFC_1123, tm_result);
    buffer[sizeof(buffer) - 1] = '\0';

    return buffer;
}

static time_t convert_from_http_date(const std::string &str)
{
    if (str.empty())
        return 0;

    struct tm time_data;
    memset(&time_data, 0, sizeof(struct tm));

    bool success = strptime(str.c_str(), HTTP_DATE_RFC_1123, &time_data)
            || strptime(str.c_str(), HTTP_DATE_RFC_850, &time_data)
            || strptime(str.c_str(), HTTP_DATE_ASCTIME, &time_data);

    if (!success)
        return 0;

    return timegm(&time_data);
}

class headers_storage
{
public:
    const std::vector<headers_entry> &get_headers() const
    {
        return m_data;
    }

    void set_headers(const std::vector<headers_entry> &headers)
    {
        m_data = headers;
    }

    void add_header(const std::string &name, const std::string &value)
    {
        m_data.emplace_back(name, value);
    }

    void set_header(const std::string &name, const std::string &value)
    {
        auto it = find_header(name.c_str(), name.size());

        if (it == m_data.end())
            m_data.emplace_back(name, value);
        else
            it->second = value;
    }

    std::vector<headers_entry>::iterator find_header(const char *name, size_t name_size)
    {
        return find_header(m_data.begin(), m_data.end(), name, name_size);
    }

    std::vector<headers_entry>::const_iterator find_header(const char *name, size_t name_size) const
    {
        return find_header(m_data.begin(), m_data.end(), name, name_size);
    }

    template <size_t N>
    std::vector<headers_entry>::const_iterator find_header(const char (&name)[N]) const
    {
        return find_header(name, N - 1);
    }

    bool has_header(const std::string &name) const
    {
        return find_header(name.c_str(), name.size()) != m_data.end();
    }

    template <size_t N>
    bool has_header(const char (&name)[N]) const
    {
        return find_header(name, N - 1) != m_data.end();
    }

    std::string get_header(const char *name, size_t name_size) const
    {
        auto it = find_header(name, name_size);

        if (it != m_data.end())
            return it->second;

        return std::string();
    }

    std::string get_header(const std::string &name) const
    {
        return get_header(name.c_str(), name.size());
    }

    template <size_t N>
    std::string get_header(const char (&name)[N]) const
    {
        return get_header(name, N - 1);
    }

    std::vector<headers_entry>::const_iterator end()
    {
        return m_data.end();
    }

    std::vector<headers_entry>::const_iterator end() const
    {
        return m_data.end();
    }

private:
    template <typename T>
    static T find_header(T begin, T end, const char *name, size_t name_size)
    {
        for (; begin != end; ++begin) {
            if (are_case_insensitive_equal(begin->first, name, name_size))
                return begin;
        }

        return end;
    }


    std::vector<headers_entry> m_data;
};

class shared_data
{
public:
    shared_data() : refcnt(0) {}
    shared_data(const shared_data &) : refcnt(0) {}

    std::atomic_int refcnt;
};

class network_request_data : public shared_data
{
public:
    network_request_data()
        : follow_location(false), timeout(30000),
          major_version(1), minor_version(1)
    {
    }
    network_request_data(const network_request_data &o)
        : url(o.url), follow_location(o.follow_location),
          timeout(o.timeout), headers(o.headers),
          major_version(o.major_version), minor_version(o.minor_version),
          method(o.method)
    {
    }

    std::string url;
    bool follow_location;
    long timeout;
    headers_storage headers;
    int major_version;
    int minor_version;
    std::string method;
};

network_request::network_request() : m_data(new network_request_data)
{
}

network_request::network_request(const network_request &other) : m_data(other.m_data)
{
}

network_request::~network_request()
{
}

network_request &network_request::operator =(const network_request &other)
{
    m_data = other.m_data;
}

const std::string &network_request::get_url() const
{
    return m_data->url;
}

void network_request::set_url(const std::string &url)
{
    m_data->url = url;
}

bool network_request::get_follow_location() const
{
    return m_data->follow_location;
}

void network_request::set_follow_location(bool follow_location)
{
    m_data->follow_location = follow_location;
}

long network_request::get_timeout() const
{
    return m_data->timeout;
}

void network_request::set_timeout(long timeout)
{
    m_data->timeout = timeout;
}

const std::vector<headers_entry> &network_request::get_headers() const
{
    return m_data->headers.get_headers();
}

bool network_request::has_header(const std::string &name) const
{
    return m_data->headers.has_header(name);
}

std::string network_request::get_header(const std::string &name) const
{
    return m_data->headers.get_header(name);
}

std::string network_request::get_header(const char *name) const
{
    return m_data->headers.get_header(name);
}

void network_request::set_headers(const std::vector<headers_entry> &headers)
{
    m_data->headers.set_headers(headers);
}

void network_request::set_header(const headers_entry &header)
{
    m_data->headers.set_header(header.first, header.second);
}

void network_request::set_header(const std::string &name, const std::string &value)
{
    m_data->headers.set_header(name, value);
}

void network_request::add_header(const headers_entry &header)
{
    m_data->headers.add_header(header.first, header.second);
}

void network_request::add_header(const std::string &name, const std::string &value)
{
    m_data->headers.add_header(name, value);
}

bool network_request::has_if_modified_since() const
{
    return has_header(IF_MODIFIED_SINCE_HEADER);
}

time_t network_request::get_if_modified_since() const
{
    std::string http_date = get_if_modified_since_string();

    if (http_date.empty())
        return 0;

    return convert_from_http_date(http_date);
}

std::string network_request::get_if_modified_since_string() const
{
    return get_header(IF_MODIFIED_SINCE_HEADER);
}

void network_request::set_if_modified_since(const std::string &time)
{
    m_data->headers.set_header(IF_MODIFIED_SINCE_HEADER, time);
}

void network_request::set_if_modified_since(time_t time)
{
    set_if_modified_since(convert_to_http_date(time));
}

void network_request::set_http_version(int major_version, int minor_version)
{
    m_data->major_version = major_version;
    m_data->minor_version = minor_version;
}

int network_request::get_http_major_version() const
{
    return m_data->major_version;
}

int network_request::get_http_minor_version() const
{
    return m_data->minor_version;
}

void network_request::set_method(const std::string &method)
{
    m_data->method = method;
}

std::string network_request::get_method() const
{
    return m_data->method;
}

void network_request::set_content_length(size_t length)
{
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%zu", length);
    m_data->headers.set_header(CONTENT_LENGTH_HEADER, buffer);
}

bool network_request::has_content_length() const
{
    return m_data->headers.has_header(CONTENT_LENGTH_HEADER);
}

size_t network_request::get_content_length() const
{
    const std::string header = m_data->headers.get_header(CONTENT_LENGTH_HEADER);

    return header.empty() ? 0ll : atoll(header.c_str());
}

void network_request::set_content_type(const std::string &type)
{
    m_data->headers.set_header(CONTENT_TYPE_HEADER, type);
}

bool network_request::has_content_type() const
{
    return m_data->headers.has_header(CONTENT_TYPE_HEADER);
}

std::string network_request::get_content_type() const
{
    return m_data->headers.get_header(CONTENT_TYPE_HEADER);
}

bool network_request::is_keep_alive() const
{
    auto header = m_data->headers.find_header(CONNECTION_HEADER);
    if (header == m_data->headers.end())
		return (m_data->major_version == 1 && m_data->minor_version == 1);

    return are_case_insensitive_equal(header->second, CONNECTION_HEADER_KEEP_ALIVE,
                                      sizeof(CONNECTION_HEADER_KEEP_ALIVE) - 1);
}

class network_reply_data : public shared_data
{
public:
    network_reply_data()
        : code(0), error(0)
    {
    }
    network_reply_data(const network_reply_data &o)
        : request(o.request), code(o.code), error(o.error),
          url(o.url), headers(o.headers), data(o.data)
    {
    }

    network_request request;

    int code;
    int error;
    std::string url;
    headers_storage headers;
    std::string data;
};

network_reply::network_reply() : m_data(new network_reply_data)
{
}

network_reply::network_reply(const network_reply &other) : m_data(other.m_data)
{
}

network_reply::~network_reply()
{
}

network_reply &network_reply::operator =(const network_reply &other)
{
    m_data = other.m_data;
}

network_request network_reply::get_request() const
{
    return m_data->request;
}

void network_reply::set_request(const network_request &request)
{
    m_data->request = request;
}

int network_reply::get_code() const
{
    return m_data->code;
}

void network_reply::set_code(int code)
{
    m_data->code = code;
}

int network_reply::get_error() const
{
    return m_data->error;
}

void network_reply::set_error(int error)
{
    m_data->error = error;
}

const std::string &network_reply::get_url() const
{
    return m_data->url;
}

void network_reply::set_url(const std::string &url)
{
    m_data->url = url;
}

const std::vector<headers_entry> &network_reply::get_headers() const
{
    return m_data->headers.get_headers();
}

bool network_reply::has_header(const std::string &name) const
{
    return m_data->headers.has_header(name);
}

std::string network_reply::get_header(const std::string &name) const
{
    return m_data->headers.get_header(name);
}

std::string network_reply::get_header(const char *name) const
{
    return m_data->headers.get_header(name);
}

void network_reply::set_headers(const std::vector<headers_entry> &headers)
{
    m_data->headers.set_headers(headers);
}

void network_reply::set_header(const headers_entry &header)
{
    m_data->headers.set_header(header.first, header.second);
}

void network_reply::set_header(const std::string &name, const std::string &value)
{
    m_data->headers.set_header(name, value);
}

void network_reply::add_header(const headers_entry &header)
{
    m_data->headers.add_header(header.first, header.second);
}

void network_reply::add_header(const std::string &name, const std::string &value)
{
    m_data->headers.add_header(name, value);
}

const std::string &network_reply::get_data() const
{
    return m_data->data;
}

void network_reply::set_data(const std::string &data)
{
    m_data->data = data;
}

bool network_reply::has_last_modified() const
{
    return has_header(LAST_MODIFIED_HEADER);
}

time_t network_reply::get_last_modified() const
{
    std::string http_date = get_last_modified_string();

    if (http_date.empty())
        return 0;

    return convert_from_http_date(http_date);
}

std::string network_reply::get_last_modified_string() const
{
    return get_header(LAST_MODIFIED_HEADER);
}

void network_reply::set_last_modified(const std::string &last_modified)
{
    m_data->headers.set_header(LAST_MODIFIED_HEADER, last_modified);
}

void network_reply::set_last_modified(time_t last_modified)
{
    set_last_modified(convert_to_http_date(last_modified));
}

void network_reply::set_content_length(size_t length)
{
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%zu", length);
    m_data->headers.set_header(CONTENT_LENGTH_HEADER, buffer);
}

bool network_reply::has_content_length() const
{
    return m_data->headers.has_header(CONTENT_LENGTH_HEADER);
}

size_t network_reply::get_content_length() const
{
    const std::string header = m_data->headers.get_header(CONTENT_LENGTH_HEADER);

    return header.empty() ? 0ll : atoll(header.c_str());
}

void network_reply::set_content_type(const std::string &type)
{
    m_data->headers.set_header(CONTENT_TYPE_HEADER, type);
}

bool network_reply::has_content_type() const
{
    return m_data->headers.has_header(CONTENT_TYPE_HEADER);
}

std::string network_reply::get_content_type() const
{
    return m_data->headers.get_header(CONTENT_TYPE_HEADER);
}

}
}
