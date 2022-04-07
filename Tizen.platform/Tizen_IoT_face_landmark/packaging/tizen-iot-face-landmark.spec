Name:       tizen-iot-face-landmark
Version:    0.0.1
Release:    0
Summary:    nnstreamer example application for face landmark on tizen platform
License:    LGPL-2.1
URL:        http://www.tizen.org/
Source:     %name-%version.tar.xz
Source1001: %name.manifest

BuildRequires: meson
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(appcore-efl)
BuildRequires: pkgconfig(gstreamer-1.0)
BuildRequires: pkgconfig(gstreamer-video-1.0)
BuildRequires: pkgconfig(cairo)
BuildRequires: pkgconfig(elementary)
BuildRequires: pkgconfig(ecore)
BuildRequires: pkgconfig(evas)
BuildRequires: pkgconfig(ecore-wl2)
BuildRequires: pkgconfig(ecore-evas)
BuildRequires: pkgconfig(tizen-extension-client)

%description
nnstreamer example application for face landmark on tizen platform

%prep
%setup -q
cp %{SOURCE1001} .

%build
CFLAGS=$(echo "${CFLAGS} -Wno-cpp" | sed 's/-O2/-O0/')
CXXFLAGS=$(echo "${CXXFLAGS} -Wno-cpp" | sed 's/-O2/-O0/')
meson setup \
    --prefix /usr \
    --bindir %{_bindir} \
    builddir
ninja -C builddir all

%install
export DESTDIR=%{buildroot}
ninja -C builddir install

%files
%manifest %{name}.manifest
%defattr(-,root,root,-)
%attr(655,root,root) %{_bindir}/*
