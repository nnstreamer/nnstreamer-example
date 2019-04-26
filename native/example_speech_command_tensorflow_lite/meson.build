executable('nnstreamer_example_speech_command_tflite',
  'nnstreamer_example_speech_command_tflite.c',
  dependencies: [glib_dep, gst_dep],
  install: true,
  install_dir: examples_install_dir
)

cf_flag = cc.has_header('nnstreamer/tensor_filter_custom.h') or nns_dep.found()
library('nnscustom_speech_command_tflite',
  'nnscustom_speech_command_tflite.c',
  install: cf_flag,
  install_dir: examples_install_dir,
  build_by_default: cf_flag
)
