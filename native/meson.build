subdir('example_cam')
subdir('example_sink')

if have_tensorflow
  subdir('example_object_detection_tensorflow')
endif

if have_tensorflow_lite
  subdir('example_image_classification_tflite')
  subdir('example_object_detection_tensorflow_lite')
  subdir('example_decoder_image_labelling')
  subdir('example_filter_performance_profile')
  subdir('example_speech_command_tensorflow_lite')
  subdir('example_two_tensor_stream')
  subdir('example_text_classification')
endif

if have_caffe2
  subdir('example_image_classification_caffe2')
endif
