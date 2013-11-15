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

#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ == 4 && __GNUC_MINOR__ < 5
/*
 * GCC-4.4 don't have <atomic> include, so we should use <cstdatomic> on it
 */
#  define SWARM_CSTDATOMIC
#  define SWARM_GCC_4_4
/*
 * GCC-4.4 don''t have noexcept support yet
 */
#  define SWARM_NOEXCEPT
#else
#  define SWARM_NOEXCEPT noexcept
#endif
