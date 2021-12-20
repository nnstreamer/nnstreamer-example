Name:		vivante-pipeline-experiment
Summary:	sample-pipeline
Version:	1.0.0
Release:	0
Group:		Applications/Multimedia
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
Tizen Native C-API Sample-pipeline with Vivante

%prep
%setup -q
cp %{SOURCE1001} .

%build
gcc src/main.c -o %{name} `pkg-config --libs --cflags capi-ml-inference glib-2.0`

%install
mkdir -p %{buildroot}%{_bindir}
install -m 755 %{name} %{buildroot}%{_bindir}/%{name}

%files
%manifest %{name}.manifest
%{_bindir}/%{name}

%changelog
* Mon May 11 2020 HyoungJoo Ahn <hello.ahn@samsung.com>
- create the various test cases of nnstreamer
