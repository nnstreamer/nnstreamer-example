%define		nnstexampledir	/usr/lib/nnstreamer/bin

Name:		tizen-iot-text-classification
Summary:	text-classification-sample-tizen-iot
Version:	1.0.0
Release:	0
Group:		Machine Learning/ML Framework
Packager:	Gichan Jang <gichan2.jang@samsung.com>
License:	LGPL-2.1
Source0:	%{name}-%{version}.tar.gz
Source1001:	%{name}.manifest

Requires:	nnstreamer
Requires:	gstreamer >= 1.8.0
Requires:	gst-plugins-good
Requires:	gst-plugins-base
BuildRequires:	meson
BuildRequires:	pkg-config
BuildRequires:	glib2-devel
BuildRequires:	gstreamer-devel
BuildRequires:	pkgconfig(gstreamer-1.0)
BuildRequires:	pkgconfig(gstreamer-app-1.0)
BuildRequires:	pkgconfig(nnstreamer)
BuildRequires:	tensorflow-lite-devel

%description
Text classification sample app with Tizen IoT platform

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
%manifest tizen-iot-text-classification.manifest
%defattr(-,root,root,-)
%{nnstexampledir}/*

%changelog
* Tue Jun 23 2020 Gichan Jang <gichan2.jang@samsung.com>
- create the Tizen IoT text classification smaple app
