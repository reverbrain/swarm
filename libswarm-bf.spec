Summary:	Swarm
Name:		libswarm
Version:	0.5.4.3
Release:	1%{?dist}

License:	GPLv2+
Group:		System Environment/Libraries
URL:		http://www.ioremap.net/projects/elliptics
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%if %{defined rhel} && 0%{?rhel} < 6
BuildRequires:	gcc44 gcc44-c++
%define boost_ver 141
%else
%define boost_ver %{nil}
%endif
BuildRequires:  libxml2-devel libev-devel
BuildRequires:	boost%{boost_ver}-devel, boost%{boost_ver}-iostreams, boost%{boost_ver}-system, boost%{boost_ver}-thread
BuildRequires:	elliptics-client-devel
BuildRequires:  curl-devel
BuildRequires:	cmake uriparser-devel

Obsoletes: srw

%description
Elliptics network is a fault tolerant distributed hash table
object storage.


%package devel
Summary: Development files for %{name}
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}


%description devel
This package contains libraries, header files and developer documentation
needed for developing software which uses the cairo graphics library.

%package -n libthevoid
Summary:	libthevoid
Group:		Development/Libraries
Requires:	libswarm = %{version}-%{release}


%description -n libthevoid
libthevoid


%package -n libthevoid-devel
Summary:	TheVoid devel
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	libthevoid = %{version}-%{release}, libswarm-devel = %{version}-%{release}

%description -n libthevoid-devel
libthevoid devel


%prep
%setup -q

%build
%if %{defined rhel} && 0%{?rhel} < 6
export CC=gcc44
export CXX=g++44
CXXFLAGS="-pthread -I/usr/include/boost141" LDFLAGS="-L/usr/lib64/boost141" %{cmake} -DBoost_LIB_DIR=/usr/lib64/boost141 -DBoost_INCLUDE_DIR=/usr/include/boost141 -DBoost_LIBRARYDIR=/usr/lib64/boost141 -DBOOST_LIBRARYDIR=/usr/lib64/boost141 -DCMAKE_CXX_COMPILER=g++44 -DCMAKE_C_COMPILER=gcc44 -DBUILD_NETWORK_MANAGER=off .
%else
%{cmake} .
%endif

make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%post -n libthevoid -p /sbin/ldconfig
%postun -n libthevoid -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_libdir}/*swarm*.so*

%files devel
%defattr(-,root,root,-)
%{_includedir}/swarm/*
%{_libdir}/*swarm*.so


%files -n libthevoid
%defattr(-,root,root,-)
%{_libdir}/*thevoid*.so*

%files -n libthevoid-devel
%defattr(-,root,root,-)
%{_includedir}/thevoid/*
%{_libdir}/*thevoid*.so


%changelog
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
