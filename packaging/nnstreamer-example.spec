Name:		nnstreamer-example
Summary:	examples of nnstreamer (gstremaer plugins for neural networks)
Version:	0.1.0
Release:	1rc1
Group:		Applications/Multimedia
Packager:	MyungJoo Ham <myungjoo.ham@samsung.com>
License:	LGPL-2.1
Source0:	nnstreamer-example-%{version}.tar.gz
Source1001:	nnstreamer-example.manifest

Requires:	nnstreamer
Requires:	gstreamer >= 1.8.0
Requires:	gst-plugins-good
Requires:	gst-plugins-good-extra
Requires:	gst-plugins-base
BuildRequires:	pkg-config
BuildRequires:	glib2-devel
BuildRequires:	gstreamer-devel
BuildRequires:	pkgconfig(gstreamer-1.0)
BuildRequires:	pkgconfig(gstreamer-video-1.0)
BuildRequires:	pkgconfig(gstreamer-audio-1.0)
BuildRequires:	pkgconfig(gstreamer-app-1.0)
BuildRequires:	pkgconfig(nnstreamer)
BuildRequires:	meson

# for tensorflow
%ifarch x86_64 aarch64
BuildRequires:	protobuf-devel >= 3.4.0
BuildRequires:	tensorflow
BuildRequires:	tensorflow-devel
%endif
# for tensorflow-lite
BuildRequires:	tensorflow-lite-devel
# for cairo
BuildRequires:	coregl-devel
BuildRequires:	cairo-devel

%define		nnstexampledir	/usr/lib/nnstreamer/bin

%description
NNStreamer is a set of gstreamer plugins to support general neural networks
and their plugins in a gstreamer stream.

%prep
%setup -q
cp %{SOURCE1001} .

%build
mkdir -p build

meson --buildtype=plain --werror --prefix=%{_prefix} --libdir=%{_libdir} --bindir=%{nnstexampledir} --includedir=%{_includedir} build
ninja -C build %{?_smp_mflags}

%install
DESTDIR=%{buildroot} ninja -C build %{?_smp_mflags} install

%files
%manifest nnstreamer-example.manifest
%defattr(-,root,root,-)
%license LICENSE
%{nnstexampledir}/*

%changelog
* Wed Jan 10 2019 MyungJoo Ham <myungjoo.ham@samsung.com>
- Packaged nnstreamer examples
