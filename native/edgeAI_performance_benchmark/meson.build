# Install performance bench mark example
executable('performance_benchmark_query',
  'tensor_query_performance_benchmark.c',
  dependencies: [glib_dep, gst_dep, gmodule_dep, nns_dep, nns_edge_dep],
  install: true,
  install_dir: examples_install_dir
)

executable('performance_benchmark_broadcast',
  'pubsub_performance_benchmark.c',
  dependencies: [glib_dep, gst_dep, gmodule_dep, nns_dep, nns_edge_dep],
  install: true,
  install_dir: examples_install_dir
)

install_data(['profiling.sh'],
  install_dir: examples_install_dir
)
