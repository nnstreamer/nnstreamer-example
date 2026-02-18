project('ml-lxm-service', 'c', 'cpp',  
  version: '0.1.0',  
  license: ['LGPL-2.1'],  
  meson_version: '>=0.50.0',  
  default_options: [  
    'werror=true',  
    'warning_level=1',  
    'c_std=gnu89',  
    'cpp_std=c++11'  
  ]  
)  

# Install paths  
prefix = get_option('prefix')  
bindir = join_paths(prefix, get_option('bindir'))  
libdir = join_paths(prefix, get_option('libdir'))  
includedir = join_paths(prefix, get_option('includedir'))  

project_inc = include_directories('include')  

# Dependencies  
glib_dep = dependency('glib-2.0')  
nns_capi_common_dep = dependency('capi-ml-common', required: true)  
ml_service_dep = dependency('capi-ml-service', required: true)  

lxm_service_sources = [  
  'src/ml-lxm-service.c'  
]  

lxm_service_lib = shared_library(  
  'ml-lxm-service',  
  sources: lxm_service_sources,  
  dependencies: [glib_dep, nns_capi_common_dep, ml_service_dep],  
  include_directories: project_inc,  
  install: true,  
  install_dir: libdir  
)  

install_headers('include/ml-lxm-service.h', subdir: 'ml-lxm-service')  

executable('ml-lxm-service-example',  
  'example/main.cc',  
  dependencies: [glib_dep, nns_capi_common_dep, ml_service_dep],  
  link_with: lxm_service_lib,  
  include_directories: project_inc,  
  install: true,  
  install_dir: bindir  
)
