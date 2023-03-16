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

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')

# Set install path
nnst_exam_prefix = get_option('prefix')
nnst_exam_bindir = join_paths(nnst_exam_prefix, get_option('bindir'))
examples_install_dir = nnst_exam_bindir

# Dependencies
glib_dep = dependency('glib-2.0')
gst_dep = dependency('gstreamer-1.0')
nns_dep = dependency('nnstreamer', required: false)
nns_edge_dep = dependency('nnstreamer-edge', required: false)

executable('nnstreamer_example_inference_offloading_edge',
  'nnstreamer_example_inference_offloading_edge.c',
  dependencies: [glib_dep, gst_dep, nns_dep, nns_edge_dep],
  install: true,
  install_dir: examples_install_dir
)

executable('nnstreamer_example_inference_offloading_leaf_pipe',
  'nnstreamer_example_inference_offloading_leaf_pipe.c',
  dependencies: [glib_dep, gst_dep, nns_dep, nns_edge_dep],
  install: true,
  install_dir: examples_install_dir
)

executable('nnstreamer_example_inference_offloading_leaf_rt',
  'nnstreamer_example_inference_offloading_leaf_rt.c',
  dependencies: [nns_edge_dep],
  install: true,
  install_dir: examples_install_dir
)
