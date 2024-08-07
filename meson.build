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
nnst_exam_libdir = join_paths(nnst_exam_prefix, get_option('libdir'))
nnst_exam_bindir = join_paths(nnst_exam_prefix, get_option('bindir'))

subplugins_install_dir = nnst_exam_libdir
examples_install_dir = nnst_exam_bindir

# Dependencies
glib_dep = dependency('glib-2.0')
gmodule_dep = dependency('gmodule-2.0')
gst_dep = dependency('gstreamer-1.0')
gst_base_dep = dependency('gstreamer-base-1.0')
gst_video_dep = dependency('gstreamer-video-1.0')
gst_audio_dep = dependency('gstreamer-audio-1.0')
gst_app_dep = dependency('gstreamer-app-1.0')

cairo_dep = dependency('cairo')
libm_dep = cc.find_library('m') # cmath library
libdl_dep = cc.find_library('dl') # DL library
thread_dep = dependency('threads') # pthread for tensorflow-lite

opencv_dep = dependency('opencv4', required: false)
protobuf_dep = dependency('protobuf', version: '>= 3.4.0', required: false)

# Dependency for custom filter examples
nns_dep = dependency('nnstreamer', required: false)

# Dependency for nnstreamer C-API examples
nns_capi_inf_dep = dependency('capi-ml-inference', required: false)
nns_capi_common_dep = dependency('capi-ml-common', required: false)

# Dependency for tensorflow example
tf_dep = dependency('tensorflow', required: false)
have_tensorflow = tf_dep.found() and protobuf_dep.found()

# Dependency for tensorflow-lite example
tflite_dep = dependency('tensorflow-lite', required: false)
tflite2_dep = dependency('tensorflow2-lite', required: false)
have_tensorflow_lite = tflite_dep.found() or tflite2_dep.found()

# Dependency for caffe2 example
caffe2_dep = dependency('caffe2', required: false)
have_caffe2 = caffe2_dep.found() and protobuf_dep.found()

# Dependency for nnfw example
nnfw_dep = dependency('nnfw', required: false)
if not nnfw_dep.found()
  nnfw_dep = cc.find_library('nnfw-dev', required: false)
endif
have_nnfw = nnfw_dep.found()

# Dependency for openvino example
openvino_dep = dependency('openvino', required: false)
have_openvino = openvino_dep.found()

# Dependency for pytorch example
pytorch_dep = dependency('pytorch', required: false)
if not pytorch_dep.found()
  pytorch_dep = dependency('torch', required: false)
endif
have_pytorch = pytorch_dep.found()

# Dependency for nnstreamer-edge examples
nns_edge_dep = dependency('nnstreamer-edge', required: false)

# Dependency for ncnn examples
ncnn_dep = dependency('ncnn', required: false)
have_ncnn = ncnn_dep.found()

# Dependency for onnxruntime examples
onnx_dep = dependency('libonnxruntime', required: false)
have_onnx = onnx_dep.found()

# Dependency for training offloading example
nntrainer_dep = dependency('nntrainer', required: false)

# Build and install nnstreamer native examples
subdir('native')

# Install nnstreamer script examples
subdir('bash_script')
