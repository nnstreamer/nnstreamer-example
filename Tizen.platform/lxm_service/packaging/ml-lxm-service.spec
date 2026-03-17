%define		nnstexampledir	/usr/lib/nnstreamer/bin

Name:		ml-lxm-service
Summary:	ML LXM Service API
Version:	1.0.0
Release:	0
Group:		Machine Learning/ML Framework
Packager:	Hyunil Park <hyunil46.park@samsung.com>
License:	LGPL-2.1
Source0:	%{name}-%{version}.tar.gz
Source1001:	%{name}.manifest

Requires:	nnstreamer
BuildRequires:	meson
BuildRequires:	pkgconfig(glib-2.0)
BuildRequires:	pkgconfig(capi-ml-service)


%description
ML LXM Service API

%prep
%setup -q
cp %{SOURCE1001} .

%build
mkdir -p build

meson --buildtype=plain --prefix=%{_prefix} --libdir=%{_libdir} --bindir=%{nnstexampledir} --includedir=%{_includedir} build
ninja -C build %{?_smp_mflags}

%install
DESTDIR=%{buildroot} ninja -C build install

find %{buildroot}%{_libdir} -name 'libml-lxm-service.so*' -print

install -d %{buildroot}%{_includedir}/ml-lxm-service

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{nnstexampledir}/*
%dir %{_includedir}/ml-lxm-service
%{_libdir}/libml-lxm-service.so*
%{_includedir}/ml-lxm-service/ml-lxm-service.h
%{nnstexampledir}/*
