Name:		vivante-yolov3-pipeline
Summary:	sample-pipeline
Version:	1.0.0
Release:	0
Group:		Applications/Multimedia
Packager:	HyoungJoo Ahn <hello.ahn@samsung.com>
License:	Proprietary
Source0:	%{name}-%{version}.tar.gz
Source1001:	%{name}.manifest

Requires:	nnstreamer-vivante
Requires:	vnn-yolo-v3
BuildRequires:	cmake
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
cmake .
make

%install
mkdir -p %{buildroot}%{_bindir}
mkdir -p %{buildroot}%{_datadir}/vivante/res
install -m 755 %{name} %{buildroot}%{_bindir}/%{name}
install -m 644 res/sample_car_bicyle_dog_416x416.bin %{buildroot}%{_datadir}/vivante/res/sample_car_bicyle_dog_416x416.bin

%files
%manifest %{name}.manifest
%{_bindir}/%{name}
%{_datadir}/vivante/res/sample_car_bicyle_dog_416x416.bin

%changelog
* Wed Mar 04 2020 HyoungJoo Ahn <hello.ahn@samsung.com>
- create the minimal sample-pipeline for yolov3
