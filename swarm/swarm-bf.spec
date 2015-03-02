# library soname
%global __soname 3

Name:		swarm
Summary:	Swarm is aiming at your web
Version:	3.0.0
Release:	1%{?dist}

License:	GPLv2+
Group:		System Environment/Libraries
URL:		https://github.com/reverbrain/swarm
Source:		https://github.com/reverbrain/swarm/archive/%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-%{version}-root

%if %{defined rhel} && 0%{?rhel} < 6
BuildRequires:	gcc44 gcc44-c++
%define boost_ver 141
%else
%define boost_ver %{nil}
%endif

BuildRequires: boost%{boost_ver}-devel
BuildRequires: boost%{boost_ver}-system-devel
BuildRequires: boost%{boost_ver}-filesystem-devel
BuildRequires: boost%{boost_ver}-thread-devel
BuildRequires: libblackhole-devel
BuildRequires: uriparser-devel
BuildRequires: libidn-devel
BuildRequires: cmake


%package -n libswarm%{__soname}
Summary: Swarm - Core library
Group: Development/Libraries

%description -n libswarm%{__soname}
Swarm is aiming at your web.


%package -n libswarm%{__soname}-devel
Summary: Swarm - Development files
Group: Development/Libraries
Requires: libswarm%{__soname} = %{version}-%{release}
Requires: boost%{boost_ver}-devel
Requires: libblackhole-devel
Provides: libswarm-devel = %{version}-%{release}
Obsoletes: libswarm-devel < 3

%description -n libswarm%{__soname}-devel
Swarm is aiming at your web.


%prep
%setup -q
#%%prep


%build
cd %{_builddir}/swarm-%{version}/%{name}
%{cmake} -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=RelWithDebInfo
make %{?_smp_mflags} -j $(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 1)

#%%build


%install
rm -rf %{buildroot}

cd %{_builddir}/swarm-%{version}/%{name}
%make_install

#%%install


%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig


%clean
rm -rf %{buildroot}
#%%clean


%files -n libswarm%{__soname}
%defattr(-,root,root,-)
%{_libdir}/libswarm.so.*


%files -n libswarm%{__soname}-devel
%defattr(-,root,root,-)
%{_includedir}/swarm/*
%{_libdir}/libswarm.so


%changelog
* Mon Feb 09 2015 Danil Osherov <shindo@yandex-team.ru> - 3.0.0
- signal handling for stopping servers removed.
- version numbering scheme changed to MAJOR.MINOR.PATCH.

* Tue Feb 03 2015 Danil Osherov <shindo@yandex-team.ru> - 0.7.3.0
- call close() on socket along with shutdown() for graceful shutdown.
- remove handler from the connection once handler's on_close() method is called.
- keep sending loop running even in case of failed writes.

* Tue Dec 23 2014 Danil Osherov <shindo@yandex-team.ru> - 0.7.2.0
- send_reply(int code) method doesn't defer close() method invocation.
- buffered_request_stream's on_chunk() method is called only when handler's ready to process it.
- * either it's first chunk or try_next_chunk() method is called.

* Mon Dec 15 2014 Danil Osherov <shindo@yandex-team.ru> - 0.7.1.0
- read no more than request's content length data from client.
- buffered_request_stream class reworked.
- reply_stream::send_error() implementation fixed according to the documentation (in comment section).
- terminate connection on invalid URL or missing handler.

* Mon Oct 20 2014 Artem Sokolov <derikon@yandex-team.ru> - 0.7.0.11
- Fixed output of http status in access log
- * There was a following case:
- * Client sent a request. Server received only a part of request and had
- * already known that request cannot be processed (either bad request, or
- * some internal error or something else). Server sent some response for
- * Client. Client got response, interrupted sending of request and
- * shutdowned socket. Thevoid tried to recive the rest of request, got an
- * error of reading from the closed socket, decided that Client went away
- * before request had processed and printed 499 http status.
- * The behaviour for that case was changed:
- * If Client gets whole response from Server, the resposne's http status
- * will be printed in access log instead of 499.

* Tue Oct 14 2014 Evgeniy Polyakov <zbr@ioremap.net> - 0.7.0.10
- sync debian/control and spec versions

* Thu Sep 11 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.7.0.8
- thevoid: Destroy io_services before server_data

* Tue Sep 02 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.7.0.7
- thevoid: Changed log levels of some messages

* Tue Sep 02 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.7.0.6
- thevoid: Set correct backlog for sockets
- example: Changed default backlog and log level
- thevoid: Changed debug logs to more user-friendly

* Fri Aug 22 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.7.0.5
- thevoid: Print request_header with new request_id
- thevoid: send_error should close connection

* Thu Aug 21 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.7.0.4
- Logger: Write logs in local time

* Wed Aug 20 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.7.0.3
- thevoid: Set valid scoped attribute
- thevoid: Added request_stream::wrap

* Mon Aug 11 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.7.0.2
- thevoid: Don't print access log twice

* Fri Aug 08 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.7.0.1
- packages: Devel package must depend on blackhole

* Fri Aug 08 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.7.0.0
- logger: Moved to blackhole
- thevoid: Added request_id and trace support
- thevoid: Introduced thevoid::http_*
- both: Moved to blackhole's platform detection
- thevoid: Changed close's semantic
- urlfetcher: Make class movable, fixed destructor
- urlfetcher: Stop timer on negative timeout

* Thu Jul 31 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.5.1
- thevoid: Get local endpoint at async_accept

* Fri Jul 11 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.5.0
- swarm: Fixed behavior of swarm::url::path
- * Return path "/my/cool/path" for "http://qutim.org/my/cool/path" instead of "my/cool/path" (slash is missed)

* Thu Jul 10 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.4.0
- thevoid: Added more server::options
- * Added path's regular expression match.
- * Added query match.
- * Added hostname match.

* Tue Jul 01 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.9
- swarm: Don't check OS_ERRNO if request successed

* Mon Jun 09 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.8
- thevoid: Added additional error messages
- * Write error log message in case of invalid urls and if handler for url is not found

* Thu Jun 05 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.7
- swarm: boost: Cancel socket on pool_remove
- swarm: Added punycode support to URLs
- * Added normal percent encoding/decoding for path and fragment parts
- * Added new dependency - libidn
- thevoid: Fixed infinity loop
- * There was infinity loop in case if user writes empty buffer to the client's socket

* Mon May 19 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.6
- thevoid: Convert endpoint to strings at the start
- * This saves us from situation when socket is already closed and we
- * want to write about it to access log

* Thu May 15 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.5
- thevoid: Added local and remote addr to access log

* Thu May 15 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.4
- swarm: Fixed http_headers::add overloa
- thevoid: Added info-level access log

* Thu May 15 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.4
- swarm: Fixed http_headers::add overloa
- thevoid: Added info-level access log

* Wed May 14 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.3
- thevoid: Break connection to client correctly
- thevoid: Added more debug logs to connection

* Tue May 06 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.2
- thevoid: Fixed daemonization if pid_file not set
- swarm: Fix in boost_event_loop::socket_request
- - Curl accidently sometimes asks to listen socket both for read and write.
- swarm: Workaround libev bug

* Fri Apr 25 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.1
- swarm: Fixed behaviour of path_components
- thevoid: Make a LOT of logs for debug
- thevoid: Make server starting more controlled

* Tue Apr 08 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.3.0
- thevoid: added ability to cast own types to buffer

* Fri Apr 04 2014 Evgeniy Polyakov <zbr@ioremap.net> - 0.6.2.1
- url: fixed boost::optional dereference
- url: added missing include
- download: added missing include

* Thu Apr 03 2014 Evgeniy Polyakov <zbr@ioremap.net> - 0.6.2.0
- url: added path_components methods
- swarm/thevoid: documentation added
- thevoid: Added close shortcut to simple_request_stream
- UrlFetcher: Add runtime check for curl version
- example: added boost::asio::signal_set to download example
- example: updated sources and comments
- urlfetcher: print socket error with appropriate log level

* Fri Feb 07 2014 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.1.3
- thevoid: Final fix for non unlinkable unix sockets

* Sun Feb 02 2014 Evgeniy Polyakov <zbr@ioremap.net> - 0.6.1.2
- Stop acceptors at end of server::run method
- UrlFetcher: Disable pipelining
- UrlFetcher: Added more debug logs to boost_event_loop

* Fri Dec 06 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.1.1
- Swarm: Fixed assignment operator for url_fetcher::request/response
- Swarm: Fixed url_query::item_value<T>
- UrlFetcher: Fixed errors handling for easy handler
- TheVoid: Fixed behaviour on 400 and 404
- TheVoid: Added safe_mode for catch users exceptions and for prevent core-dumps
- TheVoid: Added --daemonize and --pidfile options

* Tue Dec 03 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.1.0
- Swarm: Fixed segfaults in ev_event_loop
- Swarm: Added ability to reopen log file
- TheVoid: Added static_assert checks for rvalue
- * This ensures correct error messages will be printed if user will pass
- * not rvalue to send_reply/send_data instead of hundreds of gcc's lines
- TheVoid: Fixed several issues connected with signals
- TheVoid: Added support for SIGHUP

* Fri Nov 22 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.0.1
- Logger: Add log level to file interface output
- TheVoid: Added header check option
- Example: Added HTTP server example

* Tue Nov 19 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.6.0.0
- All: Lots of API changes
- Swarm: UrlFetcher and xml-support separated to libraries
- Swarm: Added boost::asio-based event loop
- Swarm: Streamed interface for reply handling
- TheVoid: Fixed LWS-headers support
- TheVoid: Config-based logger initialization
- TheVoid: Daemonization support

* Wed Oct 16 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.5.5.1
- Added error message on acceptor fail
- Added call on_close at end of stream
- Added server::get_threads_count() method
- Use only CRLF as end of the line, don't stop on single LF

* Sun Oct 13 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.5.5.0
- Removed default html data for error replies
- Added a lot more http codes
- Changed way of using io_service
- Reimplemented http parsing engine

* Mon Oct 07 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.5.4.9
- Fixed segfault in network_reply::set_header

* Thu Oct 03 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.5.4.8
- Fixed connection counter in monitoring

* Thu Aug 08 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.5.4.3
- Added socket fd to socket io

* Mon Aug 05 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.5.4.1
- Added more debug info to on_socket_event logs

* Mon Aug 05 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.5.4.0
- Added logger calls to most of methods
- Added request_stream::log method	    
- * This method is shortcut for get_server()->get_logger().log

* Fri Aug 02 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.5.3.6
- Fixed swarm/logger.h includes

* Fri Aug 02 2013 Ruslan Nigmatullin <euroelessar@yandex.ru> - 0.5.3.5
- Fixed segfault in case of / request
- Added IPv6 support by removing dns one
- * Don't resolve address from the name by magic and DNS
- Fixed a bug that occurs when either argument of query list does not have a value or query list is empty

* Tue Jul 02 2013 Kirill Smorodinnikov <shaitan@yandex-team.ru> - 0.5.1.2
- Aligned version in spec and debian/control.
- Fixed handling of exact-match handlers
- Added server::on_prefix method
- Behaviour of server::on was returned to perfect-match

* Thu Jun 27 2013 Evgeniy Polyakov <zbr@ioremap.net> - 0.5.0.0
- initial spec
