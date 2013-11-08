#!/usr/bin/python
# -*- coding: utf-8 -*-

# =============================================================================
# 2013+ Copyright (c) Kirill Smorodinnikov <shaitkir@gmail.com>
# 2013+ Copyright (c) Ruslan Nigmatullin <euroelessar@yandex.ru>
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# =============================================================================

import elliptics
import msgpack

class Options(object):
    def __init__(self):
        self.groups = None
        self.source_groups = None
        self.cache_groups = None
        self.remotes = None
        self.log_file = None
        self.log_level = None
        self.cache_list_namespace = None
        self.cache_list = None
        self.file_namespace = None
        self.file = None
        self.remove = False

def parse_options():
    from optparse import OptionParser

    options = Options()

    parser = OptionParser()
    parser.usage = "%prog type [options]"
    parser.description = __doc__
    parser.add_option("-g", "--groups", action="store", dest="groups", default=None,
                      help="Comma separated list of groups where to store cache files")
    parser.add_option("-s", "--source-groups", action="store", dest="source_groups", default=None,
                      help="Comma separated list of groups of original files")
    parser.add_option("-c", "--cache-groups", action="store", dest="cache_groups", default=None,
                      help="Comma separated list of groups of cache files")
    parser.add_option("-l", "--log", dest="log_file", default='/dev/stderr', metavar="FILE",
                      help="Output log messages from library to file [default: %default]")
    parser.add_option("-L", "--log-level", action="store", dest="log_level", default="1",
                      help="Elliptics client verbosity [default: %default]")
    parser.add_option("-r", "--remote", action="append", dest="remote",
                      help="Elliptics node address [default: %default]")
    parser.add_option("--cache-list", action="store", dest="cache_list", default=None,
                      help="Copy this key from source to cache groups")
    parser.add_option("--cache-list-namespace", action="store", dest="cache_list_namespace", default=None,
                      help="Elliptics session namespace for cache list key")
    parser.add_option("--file", action="store", dest="file", default=None,
                      help="Copy this key from source to cache groups")
    parser.add_option("--file-namespace", action="store", dest="file_namespace", default=None,
                      help="Elliptics session namespace for file key")
    parser.add_option("--remove", action="store_true", dest="remove", default=False,
                      help="Remove file from cache groups instead of copying")

    (parsed_options, args) = parser.parse_args()

    if not parsed_options.groups:
        raise ValueError("Please specify at least one group (-g option)")

    def parse_groups(string):
        try:
            return map(int, string.split(','))
        except Exception as e:
            raise ValueError("Can't parse groups list: '{0}': {1}".format(parsed_options.groups, repr(e)))

    options.groups = parse_groups(parsed_options.groups)
    print("Using groups list: {0}".format(options.groups))
    options.source_groups = parse_groups(parsed_options.source_groups)
    print("Using source groups list: {0}".format(options.source_groups))
    options.cache_groups = parse_groups(parsed_options.cache_groups)
    print("Using cache groups list: {0}".format(options.cache_groups))

    try:
        options.log_file = parsed_options.log_file
        options.log_level = int(parsed_options.log_level)
    except Exception as e:
        raise ValueError("Can't parse log_level: '{0}': {1}".format(parsed_options.log_level, repr(e)))

    print("Using elliptics client log level: {0}".format(options.log_level))

    if not parsed_options.remote:
        raise ValueError("Please specify at least one remote address (-r option)")
    try:
        options.remotes = []
        for r in parsed_options.remote:
            options.remotes.append(elliptics.Address.from_host_port_family(r))
            print("Using remote host:port:family: {0}".format(options.remotes[-1]))
    except Exception as e:
        raise ValueError("Can't parse host:port:family: '{0}': {1}".format(parsed_options.remote, repr(e)))

    if not parsed_options.cache_list:
        raise ValueError("Please specify cache list key (--cache-list option)")

    if not parsed_options.file:
        raise ValueError("Please specify file key (--file option)")

    options.cache_list = parsed_options.cache_list
    options.cache_list_namespace = parsed_options.cache_list_namespace
    options.file = parsed_options.file
    options.file_namespace = parsed_options.file_namespace
    options.remove = parsed_options.remove

    return options

if __name__ == '__main__':
    options = parse_options()

    logger = elliptics.Logger(options.log_file, options.log_level)
    node = elliptics.Node(logger)

    any_remote = False

    for remote in options.remotes:
        try:
            node.add_remote(remote)
            any_remote = True
        except Exception as e:
            print("Couldn't connect to remote: {0} got: {1}".format(remote, e))

    if not any_remote:
        raise ValueError("Couldn't connect to any remote")

    # cache_list = receive_list(node, options)

    list_session = elliptics.Session(node)
    list_session.groups = options.groups
    if options.cache_list_namespace:
        list_session.set_namespace(options.cache_list_namespace)

    source_session = elliptics.Session(node)
    source_session.groups = options.source_groups
    source_session.namespace = options.file_namespace

    cache_session = elliptics.Session(node)
    cache_session.groups = options.cache_groups
    cache_session.namespace = options.file_namespace

    file_key = source_session.transform(options.file)
    file_id = ''.join(chr(x) for x in file_key.id)

    if not options.remove:
        print("Add {0} to groups {1}".format(options.file, options.cache_groups))

        read_results = source_session.read_data(file_key)
        read_results.wait()
        read_result = read_results.get()[0]

        io_attr = elliptics.IoAttr()
        io_attr.id = read_result.id
        io_attr.timestamp = read_result.timestamp
        io_attr.user_flags = read_result.user_flags

        write_result = cache_session.write_data(io_attr, read_result.data)
        write_result.wait()

        def add_file(data):
            cache_list = msgpack.unpackb(data)
            cache_list[file_id] = options.cache_groups
            return msgpack.packb(cache_list)

        list_session.write_cas(options.cache_list, add_file, 0, 3)
    else:
        print("Remove {0} from groups {1}".format(options.file, options.cache_groups))
        remove_result = cache_session.remove(file_key)
        remove_result.wait()

        def remove_file(data):
            cache_list = msgpack.unpackb(data)
            del cache_list[file_id]
            return msgpack.packb(cache_list)

        list_session.write_cas(options.cache_list, remove_file, 0, 3)

