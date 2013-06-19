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

#ifndef COCAINE_SERVICE_NETWORKREQUEST_H
#define COCAINE_SERVICE_NETWORKREQUEST_H

#include <vector>
#include <string>
#include <utility>

namespace ioremap {
namespace swarm {

class network_request_data;
class network_reply_data;

template <typename T>
class shared_data_ptr
{
public:
    explicit shared_data_ptr(T *data) : m_data(data)
    {
        if (m_data)
            ++m_data->refcnt;
    }
    shared_data_ptr() : m_data(NULL) {}
    shared_data_ptr(const shared_data_ptr &other) : m_data(other.m_data)
    {
        if (m_data)
            ++m_data->refcnt;
    }
    ~shared_data_ptr()
    {
        if (m_data && --m_data->refcnt == 0)
            delete m_data;
    }
    shared_data_ptr &operator =(const shared_data_ptr &other)
    {
        shared_data_ptr tmp(other);
        std::swap(tmp.m_data, m_data);
        return *this;
    }

    T *operator ->() { detach(); return m_data; }
    const T *operator ->() const { return m_data; }
    T &operator *() { detach(); return *m_data; }
    const T &operator *() const { return *m_data; }
    T *data() { detach(); return m_data; }
    T *data() const { return m_data; }
    T *constData() { return m_data; }

private:
    void detach()
    {
        if (m_data && m_data->refcnt != 1) {
            shared_data_ptr tmp(new T(*m_data));
            std::swap(tmp.m_data, m_data);
        }
    }

    T *m_data;
};

typedef std::pair<std::string, std::string> headers_entry;

class network_request
{
public:
    network_request();
    network_request(const network_request &other);
    ~network_request();

    network_request &operator =(const network_request &other);

    // Request URL
    const std::string &get_url() const;
    void set_url(const std::string &url);
    // Follow Location from 302 HTTP replies
    bool get_follow_location() const;
    void set_follow_location(bool follow_location);
    // Timeout in ms
    long get_timeout() const;
    void set_timeout(long timeout);
    // List of headers
    const std::vector<headers_entry> &get_headers() const;
    bool has_header(const std::string &name) const;
    std::string get_header(const std::string &name) const;
    std::string get_header(const char *name) const;
    void set_headers(const std::vector<headers_entry> &headers);
    void set_header(const headers_entry &header);
    void set_header(const std::string &name, const std::string &value);
    void add_header(const headers_entry &header);
    void add_header(const std::string &name, const std::string &value);
    // If-Modified-Since, UTC
    bool has_if_modified_since() const;
    time_t get_if_modified_since() const;
    std::string get_if_modified_since_string() const;
    void set_if_modified_since(const std::string &time);
    void set_if_modified_since(time_t time);
    // TheVoid specific arguments
    void set_http_version(int major_version, int minor_version);
    int get_http_major_version() const;
    int get_http_minor_version() const;

    void set_method(const std::string &method);
    std::string get_method() const;

    void set_content_length(size_t length);
    bool has_content_length() const;
	size_t get_content_length() const;

    void set_content_type(const std::string &type);
    bool has_content_type() const;
	std::string get_content_type() const;

	bool is_keep_alive() const;

private:
    shared_data_ptr<network_request_data> m_data;
};

class network_reply
{
public:
    enum status_type {
		ok = 200,
		created = 201,
		accepted = 202,
		no_content = 204,
		multiple_choices = 300,
		moved_permanently = 301,
		moved_temporarily = 302,
		not_modified = 304,
		bad_request = 400,
		unauthorized = 401,
		forbidden = 403,
		not_found = 404,
		internal_server_error = 500,
		not_implemented = 501,
		bad_gateway = 502,
		service_unavailable = 503
	};

    network_reply();
    network_reply(const network_reply &other);
    ~network_reply();

    network_reply &operator =(const network_reply &other);

     // Original request
    network_request get_request() const;
    void set_request(const network_request &request);

    // HTTP code
    int get_code() const;
    void set_code(int code);
    // Errno
    int get_error() const;
    void set_error(int error);
    // Final URL from HTTP reply
    const std::string &get_url() const;
    void set_url(const std::string &url);
    // List of headers
    const std::vector<headers_entry> &get_headers() const;
    bool has_header(const std::string &name) const;
    std::string get_header(const std::string &name) const;
    std::string get_header(const char *name) const;
    void set_headers(const std::vector<headers_entry> &headers);
    void set_header(const headers_entry &header);
    void set_header(const std::string &name, const std::string &value);
    void add_header(const headers_entry &header);
    void add_header(const std::string &name, const std::string &value);
    // Reply data
    const std::string &get_data() const;
    void set_data(const std::string &data);
    // Last-Modified, UTC
    bool has_last_modified() const;
    time_t get_last_modified() const;
    std::string get_last_modified_string() const;
    void set_last_modified(const std::string &last_modified);
    void set_last_modified(time_t last_modified);

    // Content length
    void set_content_length(size_t length);
    bool has_content_length() const;
	size_t get_content_length() const;

    // Content type
    void set_content_type(const std::string &type);
    bool has_content_type() const;
	std::string get_content_type() const;

private:
    shared_data_ptr<network_reply_data> m_data;
};

}
}

#endif // COCAINE_SERVICE_NETWORKREQUEST_H
