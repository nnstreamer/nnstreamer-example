Name:		vivante-inceptionv3-pipeline
Summary:	sample-pipeline
Version:	1.0.0
Release:	0
Group:		Applications/Multimedia
Packager:	HyoungJoo Ahn <hello.ahn@samsung.com>
License:	Priprietary
Source0:	%{name}-%{version}.tar.gz
Source1001:	%{name}.manifest

Requires:	nnstreamer-vivante
Requires:	vivante-neural-network-models
BuildRequires:	glib2-devel
BuildRequires:  capi-base-common-devel
BuildRequires:	pkgconfig(nnstreamer)
BuildRequires:	capi-nnstreamer-devel

%description
Tizen Native C-API Sample-pipeline with Vivante

%prep
%setup -q
cp %{SOURCE1001} .

%build
export C_INCLUDE_PATH=$C_INCLUDE_PATH:/usr/include/glib-2.0/:/usr/lib/glib-2.0/include/:/usr/include/nnstreamer/
gcc src/main.c -o %{name} -lglib-2.0 -lcapi-nnstreamer

%install
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_datadir}/vivante/res
install -m 755 %{name} %{buildroot}%{_bindir}/%{name}
install -m 644 res/sample_pizza_299x299/jpg %{buildroot}%{_datadir}/vivante/res/sample_pizza_299x299.jpg

%files
%manifest %{name}.manifest
%{_bindir}/%{name}
%{_datadir}/vivante/res/sample_pizza_299x299.jpg

%changelog
* Tue Feb 18 2020 HyoungJoo Ahn <hello.ahn@samsung.com>
- create the minimal sample-pipeline
