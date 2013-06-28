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
BuildRequires:	boost-devel
BuildRequires:	elliptics-devel
BuildRequires:  curl-devel
BuildRequires:	cmake28 uriparser-devel

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
%{cmake28} .

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
