/*
 * Copyright 2013+ Ruslan Nigmatullin <euroelessar@yandex.ru>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef IOREMAP_SWARM_STREAM_HPP
#define IOREMAP_SWARM_STREAM_HPP

#include "url_fetcher.hpp"

namespace ioremap {
namespace swarm {

/*!
 * \brief The simple_stream class makes possible to handle only final result for the requests.
 */
class simple_stream : public base_stream
{
public:
	typedef std::function<void (const url_fetcher::response &response, const std::string &data, const boost::system::error_code &error)> handler_func;

	/*!
	 * \brief Cosntructs simple_stream with \a handler.
	 *
	 * Once request is finished \a handler will be called.
	 *
	 * \attention If handler_func is non-copyable object you may use std::ref to pass it as a reference.
	 * In this case you must guarantee that handler will be alive during the whole request execution.
	 */
	simple_stream(const handler_func &handler) : m_handler(handler)
	{
	}

	static std::shared_ptr<simple_stream> create(const handler_func &handler)
	{
		return std::make_shared<simple_stream>(handler);
	}

protected:
	/*!
	 * \internal
	 */
	virtual void on_headers(url_fetcher::response &&response)
	{
		m_response = std::move(response);
		if (auto content_length = m_response.headers().content_length())
			m_data.reserve(*content_length);
	}

	/*!
	 * \internal
	 */
	virtual void on_data(const boost::asio::const_buffer &buffer)
	{
		auto data = boost::asio::buffer_cast<const char *>(buffer);
		auto size = boost::asio::buffer_size(buffer);

		m_data.append(data, data + size);
	}

	/*!
	 * \internal
	 */
	virtual void on_close(const boost::system::error_code &error)
	{
		m_handler(m_response, m_data, error);
	}

private:
	ioremap::swarm::url_fetcher::response m_response;
	std::string m_data;
	handler_func m_handler;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_STREAM_HPP
