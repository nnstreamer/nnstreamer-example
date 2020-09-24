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
gst_app_dep = dependency('gstreamer-app-1.0')

# Dependency for custom filter examples
nns_dep = dependency('nnstreamer', required: false)

# Dependency for tensorflow-lite example
tflite_dep = dependency('tensorflow-lite', required: false)

have_tensorflow_lite = false
if tflite_dep.found()
  have_tensorflow_lite = true
endif

# Build and install nnstreamer native examples
subdir('src')
subdir('res')
