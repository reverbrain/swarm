Summary:	Swarm
Name:		libswarm
Version:	0.5.0.0
Release:	1%{?dist}

License:	GPLv2+
Group:		System Environment/Libraries
URL:		http://www.ioremap.net/projects/elliptics
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  libxml2-devel libev-devel
%if %{defined rhel} && 0%{?rhel} < 6
%define boost_ver 141
%else
%define boost_ver %{nil}
%endif
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


%description -n libthevoid
libthevoid


%package -n libthevoid-devel
Summary:	TheVoid devel
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	libthevoid = %{version}-%{release}

%description -n libthevoid-devel
libthevoid devel


%prep
%setup -q

%build
%if %{defined rhel} && 0%{?rhel} < 6
CXXFLAGS="-pthread -I/usr/include/boost141" LDFLAGS="-L/usr/lib64/boost141" %{cmake} -DBoost_LIB_DIR=/usr/lib64/boost141 -DBoost_INCLUDE_DIR=/usr/include/boost141 -DBoost_LIBRARYDIR=/usr/lib64/boost141 -DBOOST_LIBRARYDIR=/usr/lib64/boost141 .
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
%{_libdir}/*swarpm*.so*

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
* Thu Jun 27 2013 Evgeniy Polyakov <zbr@ioremap.net> - 0.5.0.0
- initial spec
