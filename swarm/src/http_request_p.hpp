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

#ifndef IOREMAP_SWARM_HTTP_REQUEST_P_HPP
#define IOREMAP_SWARM_HTTP_REQUEST_P_HPP

#include <swarm/http_request.hpp>

namespace ioremap {
namespace swarm {

class http_request_data
{
public:
	http_request_data()
	{
	}

	virtual ~http_request_data()
	{
	}

	swarm::url url;
	http_headers headers;
	std::string method;
};

} // namespace swarm
} // namespace ioremap

#endif // IOREMAP_SWARM_HTTP_REQUEST_P_HPP
