# Install tensor query example
if nns_dep.found() and nns_capi_dep.found()
example_early_exit_capi = executable('nnstreamer_example_query_object_detection',
  'example_query_object_detection.c',
  dependencies: [glib_dep, gst_dep, gmodule_dep, nns_dep, nns_capi_dep],
  install: true,
  install_dir: examples_install_dir
)
endif
