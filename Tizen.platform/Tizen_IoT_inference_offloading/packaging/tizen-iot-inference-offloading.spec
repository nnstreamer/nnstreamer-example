%define		nnstexampledir	/usr/lib/nnstreamer/bin

Name:		tizen-iot-inference-offloading
Summary:	inference-offloading-sample-tizen-iot
Version:	1.0.0
Release:	0
Group:		Machine Learning/ML Framework
Packager:	Gichan Jang <gichan2.jang@samsung.com>
License:	LGPL-2.1
Source0:	%{name}-%{version}.tar.gz
Source1001:	%{name}.manifest

Requires:	nnstreamer
Requires:	nnstreamer-edge
Requires:	gstreamer >= 1.8.0
Requires:	gst-plugins-good
Requires:	gst-plugins-base
BuildRequires:	meson
BuildRequires:	pkg-config
BuildRequires:	glib2-devel
BuildRequires:	gstreamer-devel
BuildRequires:	pkgconfig(gstreamer-1.0)
BuildRequires:	pkgconfig(nnstreamer)
BuildRequires:	pkgconfig(nnstreamer-edge)
BuildRequires:	tensorflow-lite-devel

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
%manifest tizen-iot-inference-offloading.manifest
%defattr(-,root,root,-)
%{nnstexampledir}/*

%changelog
* Wed Mar 22 2023 Gichan Jang <gichan2.jang@samsung.com>
- create the inference offloading smaple app
