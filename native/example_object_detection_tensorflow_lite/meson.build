nnstreamer_example_object_detection_tflite = executable('nnstreamer_example_object_detection_tflite',
  'nnstreamer_example_object_detection_tflite.cc',
  dependencies: [glib_dep, gst_dep, gst_video_dep, cairo_dep, libm_dep],
  install: true,
  install_dir: examples_install_dir
)

# Install python example
install_data('nnstreamer_example_object_detection_tflite.py',
  install_dir: examples_install_dir
)
