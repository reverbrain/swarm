/*
 * Copyright 2015+ Danil Osherov <shindo@yandex-team.ru>
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

#ifndef IOREMAP_THEVOID_TESTS_HANDLERS_FACTORY_HPP
#define IOREMAP_THEVOID_TESTS_HANDLERS_FACTORY_HPP

#include <string>
#include <map>
#include <memory>

#include "thevoid/streamfactory.hpp"

#include "server.hpp"


namespace handlers {

typedef std::map<std::string, std::shared_ptr<ioremap::thevoid::base_stream_factory>> factory_t;

extern factory_t factory;

} // namespace handlers


#define REGISTER_HANDLER(handler_name) \
__attribute__((constructor(2000))) \
static \
void init_handlers_##handler_name() { \
	handlers::factory[#handler_name] = \
		handlers::factory_t::mapped_type( \
			new ioremap::thevoid::stream_factory<server, handlers::handler_name>(server_ptr.get()) \
		); \
}

#endif // IOREMAP_THEVOID_TESTS_HANDLERS_FACTORY_HPP
