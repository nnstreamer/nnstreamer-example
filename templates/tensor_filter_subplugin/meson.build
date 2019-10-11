project('nnstreamer', 'c',
  version: '1.0.0',
  license: ['Proprietary'],
  meson_version: '>=0.42.0',
)

cc = meson.get_compiler('c')

path_prefix = get_option('prefix')
subplugin_install_prefix = join_paths(path_prefix, 'lib', 'nnstreamer')
filter_subplugin_install_dir = join_paths(subplugin_install_prefix, 'filters')

glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')
nnstreamer_dep = dependency('nnstreamer')

base_deps = [
  glib_dep,
  gobject_dep,
  nnstreamer_dep
]

sources = [
  'src/tensor_filter_subplugin.c'
]

subplugin_shared = shared_library('TEMPLATE',
  sources,
  dependencies: base_deps,
  install: true,
  install_dir: filter_subplugin_install_dir
)
