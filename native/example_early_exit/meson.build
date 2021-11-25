# Install early-exit example
if nns_dep.found()
example_early_exit = executable('nnstreamer_example_early_exit',
  'example_early_exit.c',
  dependencies: [glib_dep, gst_dep, gmodule_dep, nns_dep],
  install: true,
  install_dir: examples_install_dir
)
if nns_capi_inf_dep.found() and nns_capi_common_dep.found()
example_early_exit_capi = executable('nnstreamer_example_early_exit_capi',
  'example_early_exit_capi.c',
  dependencies: [glib_dep, gst_dep, gmodule_dep, nns_dep, nns_capi_inf_dep, nns_capi_common_dep],
  install: true,
  install_dir: examples_install_dir
)
endif
endif
