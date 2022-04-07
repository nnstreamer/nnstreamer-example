app_deps = [
    dependency('cairo', required : true),
    dependency('appcore-efl', required: true),
    dependency('gstreamer-1.0', required: true),
    dependency('gstreamer-video-1.0', required: true),
    dependency('evas',required: true),
    dependency('ecore', required: true),
    dependency('elementary',required: true),
    dependency('ecore-evas', required: true),
    dependency('ecore-wl2', required: true),
    dependency('tizen-extension-client', required: true)
]

app_src = [
    'main.c',
]

executable('tizen_iot_face_landmark', app_src,
           dependencies : app_deps,
           install : true,
           install_dir : dir_bin)
