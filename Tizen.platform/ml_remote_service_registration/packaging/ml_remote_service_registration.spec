%define		nnstexampledir	/usr/lib/nnstreamer/bin

Name:		ml_remote_service_registration
Summary:	ml-remote-service registration sample app
Version:	1.0.0
Release:	0
Group:		Applications/Multimedia
Packager:	Gichan Jang <gichan2.jang@samsung.com>
License:	LGPL-2.1
Source0:	%{name}-%{version}.tar.gz
Source1001:	%{name}.manifest

Requires:	nnstreamer
Requires:	nnstreamer-edge

BuildRequires:	meson
BuildRequires:	pkg-config
BuildRequires:	glib2-devel
BuildRequires:	pkgconfig(nnstreamer-edge)
BuildRequires:	tensorflow-lite-devel
BuildRequires:	capi-machine-learning-service-devel
BuildRequires:	capi-machine-learning-tizen-internal-devel


%description
Inference offloading sample app with Tizen IoT platform

%prep
%setup -q
cp %{SOURCE1001} .

%build
mkdir -p build

meson --buildtype=plain --prefix=%{_prefix} --libdir=%{_libdir} --bindir=%{nnstexampledir} --includedir=%{_includedir} build
ninja -C build %{?_smp_mflags}

%install
DESTDIR=%{buildroot} ninja -C build install

%files
%manifest ml_remote_service_registration.manifest
%defattr(-,root,root,-)
%{nnstexampledir}/*

%changelog
* Fri Sep 08 2023 Gichan Jang <gichan2.jang@samsung.com>
- create the ml remote service registration smaple app
