if have_tensorflow
  subdir('example_object_detection_tensorflow')
endif

if have_tensorflow_lite
  subdir('example_object_detection_tensorflow_lite')
  subdir('example_image_segmentation_tensorflow_lite')
  if have_openvino
    subdir('example_tensorif')
  endif
endif

if have_openvino
  subdir('example_face_detection_openvino')
  subdir('example_person_detection_openvino')
endif
subdir('example_models')
subdir('example_join')
subdir('example_early_exit')
