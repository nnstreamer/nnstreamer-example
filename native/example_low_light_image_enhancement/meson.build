if opencv_dep.found()
  nstreamer_example_low_light_image_enhancement = executable('nnstreamer_example_low_light_image_enhancement',
    'nnstreamer_example_low_light_image_enhancement.cc',
    dependencies: [glib_dep, gst_dep, gst_app_dep, opencv_dep],
    install: true,
    install_dir: examples_install_dir
  )
endif

# Install python example
install_data('nnstreamer_example_low_light_image_enhancement.py',
  install_dir: examples_install_dir
)
