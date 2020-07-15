nnstreamer_example_image_classification_nnfw = executable('nnstreamer_example_image_classification_nnfw',
  'nnstreamer_example_image_classification_nnfw.c',
  dependencies: [glib_dep, gst_dep, nnfw_dep],
  install: true,
  install_dir: examples_install_dir
)

# Install MANIFEST file
nnfw_manifest_dir = join_paths(examples_install_dir, 'tflite_model_img', 'metadata')
install_data('MANIFEST',
  install_dir: nnfw_manifest_dir
)
