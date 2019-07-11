project('nnstreamer-example', 'c', 'cpp',
  version: '0.1.0',
  license: ['LGPL'],
  meson_version: '>=0.40.0',
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
nnst_exam_libdir = join_paths(nnst_exam_prefix, get_option('libdir'))
nnst_exam_bindir = join_paths(nnst_exam_prefix, get_option('bindir'))

subplugins_install_dir = nnst_exam_libdir
examples_install_dir = nnst_exam_bindir

# Dependencies
glib_dep = dependency('glib-2.0')
gst_dep = dependency('gstreamer-1.0')
gst_base_dep = dependency('gstreamer-base-1.0')
gst_video_dep = dependency('gstreamer-video-1.0')
gst_audio_dep = dependency('gstreamer-audio-1.0')
gst_app_dep = dependency('gstreamer-app-1.0')

cairo_dep = dependency('cairo')
libm_dep = cc.find_library('m') # cmath library
libdl_dep = cc.find_library('dl') # DL library
thread_dep = dependency('threads') # pthread for tensorflow-lite

# Dependency for custom filter examples
nns_dep = dependency('nnstreamer', required: false)

# Dependency for tensorflow example
tf_dep = dependency('tensorflow', required: false)
protobuf_dep = dependency('protobuf', version: '>= 3.4.0', required: false)

have_tensorflow = false
if tf_dep.found() and protobuf_dep.found()
  have_tensorflow = true
endif

# Dependency for tensorflow-lite example
tflite_dep = dependency('tensorflow-lite', required: false)

have_tensorflow_lite = false
if tflite_dep.found()
  have_tensorflow_lite = true
endif

# Dependency for caffe2 example
caffe2_dep = dependency('caffe2', required: false)

have_caffe2 = false
if caffe2_dep.found() and protobuf_dep.found()
  have_caffe2 = true
endif

# Build and install nnstreamer native examples
subdir('native')

# Install nnstreamer script examples
subdir('bash_script')
