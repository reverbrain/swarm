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

#include "event_loop.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ioremap {
namespace swarm {


event_listener::~event_listener()
{
}

static blackhole::attribute::set_t create_attributes()
{
	blackhole::attribute::set_t attributes = {
		keyword::source() = "event_loop"
	};

	return attributes;
}

event_loop::event_loop(const swarm::logger &logger) : m_logger(logger, create_attributes()), m_listener(NULL)
{
}

event_loop::~event_loop()
{
}

void event_loop::set_listener(event_listener *listener)
{
	m_listener = listener;
}

event_listener *event_loop::listener() const
{
	return m_listener;
}

const logger &event_loop::logger() const
{
	return m_logger;
}

int event_loop::open_socket(int domain, int type, int protocol)
{
	return ::socket(domain, type, protocol);
}

int event_loop::close_socket(int fd)
{
	return ::close(fd);
}

}} // namespace ioremap::swarm
