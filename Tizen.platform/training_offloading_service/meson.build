project('nnstreamer-example', 'c', 'cpp',
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

# Set install path
nnst_exam_prefix = get_option('prefix')
nnst_exam_bindir = join_paths(nnst_exam_prefix, get_option('bindir'))
examples_install_dir = nnst_exam_bindir

# Dependencies
glib_dep = dependency('glib-2.0')
nns_edge_dep = dependency('nnstreamer-edge', required: false)
nns_capi_common_dep = dependency('capi-ml-common', required: false)
ml_service_dep = dependency('capi-ml-service', required: false)

executable('training_offloadinge_service',
  'main.c',
  dependencies: [glib_dep, nns_edge_dep, nns_capi_common_dep, ml_service_dep],
  install: true,
  install_dir: examples_install_dir
)

