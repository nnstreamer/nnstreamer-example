%define		nnstexampledir	/usr/lib/nnstreamer/bin

Name:		training_offloading_service
Summary:	training_offloadingi_service app
Version:	1.0.0
Release:	0
Group:		Machine Learning/ML Framework
Packager:	Hyunil Park <hyunil46.park@samsung.com>
License:	LGPL-2.1
Source0:	%{name}-%{version}.tar.gz
Source1001:	%{name}.manifest

Requires:	nnstreamer
Requires:	nnstreamer-edge
BuildRequires:	meson
BuildRequires:	pkgconfig(glib-2.0)
BuildRequires:	pkgconfig(capi-ml-service)


%description
training offloading service sample app with Tizen IoT platform

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
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{nnstexampledir}/*

