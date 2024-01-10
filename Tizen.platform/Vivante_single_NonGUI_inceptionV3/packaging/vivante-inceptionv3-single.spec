Name:		vivante-inceptionv3-single
Summary:	sample-single
Version:	1.0.0
Release:	0
Group:		Machine Learning/ML Framework
Packager:	HyoungJoo Ahn <hello.ahn@samsung.com>
License:	LGPL-2.1
Source0:	%{name}-%{version}.tar.gz
Source1001:	%{name}.manifest

Requires:	nnstreamer-vivante
Requires:	vivante-neural-network-models
BuildRequires:	glib2-devel
BuildRequires:  capi-base-common-devel
BuildRequires:	pkgconfig(nnstreamer)
BuildRequires:	pkgconfig(capi-ml-inference)

%description
Tizen Native C-API Sample-single with Vivante

%prep
%setup -q
cp %{SOURCE1001} .

%build
gcc src/main.c -o %{name} `pkg-config --libs --cflags capi-ml-inference glib-2.0`

%install
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_datadir}/vivante/res
install -m 755 %{name} %{buildroot}%{_bindir}/%{name}
install -m 644 res/sample_pizza_299x299 %{buildroot}%{_datadir}/vivante/res/sample_pizza_299x299

%files
%manifest %{name}.manifest
%{_bindir}/%{name}
%{_datadir}/vivante/res/sample_pizza_299x299

%changelog
* Thu Feb 20 2020 HyoungJoo Ahn <hello.ahn@samsung.com>
- create the minimal sample-single
