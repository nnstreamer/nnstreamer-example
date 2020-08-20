#!/usr/bin/env bash

print_usage()
{
  echo -e "usage: $0 [model_name]"
  echo -e "model_name\timage-classification-tflite"
  echo -e "\t\timage-classification-caffe2"
  echo -e "\t\tobject-detection-tf"
  echo -e "\t\tobject-detection-tflite"
  echo -e "\t\tspeech-command"
  echo -e "\t\timage-segmentation-tflite"
  echo -e "\t\ttext-classification-tflite"
  echo -e "\t\tpose-estimation-tflite"
  echo -e "\t\tperson-detection-openvino"
  echo -e "\t\tface-detection-openvino"
  exit 1
}

if echo $0 | grep -q -w "get-model-image-classification-tflite.sh"; then
  model="image-classification-tflite"

else
  if [ -n "$1" ]; then
    model=$1
  else
    print_usage
  fi
fi

if [[ ${model} == "image-classification-tflite" ]]; then
  mkdir -p tflite_model_img
  cd tflite_model_img
  wget https://storage.googleapis.com/download.tensorflow.org/models/tflite/mobilenet_v1_1.0_224_quant_and_labels.zip
  unzip mobilenet_v1_1.0_224_quant_and_labels.zip
  rm mobilenet_v1_1.0_224_quant_and_labels.zip
  mv labels_mobilenet_quant_v1_224.txt labels.txt

elif [[ ${model} == "image-classification-caffe2" ]]; then
  mkdir -p caffe2_model/tempdir
  cd caffe2_model/tempdir
  wget -c http://dl.caffe.berkeleyvision.org/caffe_ilsvrc12.tar.gz
  tar -zxf caffe_ilsvrc12.tar.gz
  mv synset_words.txt ../
  cd .. && rm -rf tempdir
  download_url="https://s3.amazonaws.com/download.caffe2.ai/models/mobilenet_v2"
  wget ${download_url}/predict_net.pb
  wget ${download_url}/init_net.pb
  mv synset_words.txt labels.txt

elif [[ ${model} == "object-detection-tf" ]]; then
  mkdir -p tf_model
  cd tf_model
  download_url="https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow/ssdlite_mobilenet_v2"
  wget ${download_url}/ssdlite_mobilenet_v2.pb
  wget ${download_url}/coco_labels_list.txt

elif [[ ${model} == "object-detection-tflite" ]]; then
  mkdir -p tflite_model
  cd tflite_model
  download_url="https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco"
  wget ${download_url}/ssd_mobilenet_v2_coco.tflite
  wget ${download_url}/coco_labels_list.txt
  wget ${download_url}/box_priors.txt

elif [[ ${model} == "speech-command" ]]; then
  mkdir -p speech_model
  cd speech_model
  wget https://storage.googleapis.com/download.tensorflow.org/models/tflite/conv_actions_tflite.zip
  unzip conv_actions_tflite.zip
  rm conv_actions_tflite.zip

elif [[ ${model} == "image-segmentation-tflite" ]]; then
  mkdir -p tflite_img_segment_model
  cd tflite_img_segment_model
  wget https://storage.googleapis.com/download.tensorflow.org/models/tflite/gpu/deeplabv3_257_mv_gpu.tflite

elif [[ ${model} == "text-classification-tflite" ]]; then
  mkdir -p tflite_text_classification
  cd tflite_text_classification
  download_url="https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow-lite/text_classification_tflite"
  wget ${download_url}/text_classification.tflite
  wget ${download_url}/labels.txt
  wget ${download_url}/vocab.txt

elif [[ ${model} == "pose-estimation-tflite" ]]; then
  mkdir -p tflite_pose_estimation
  cd tflite_pose_estimation
  wget https://storage.googleapis.com/download.tensorflow.org/models/tflite/posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite
  echo -e "nose\nleftEye\nrightEye\nleftEar\nrightEar\nleftShoulder\nrightShoulder\nleftElbow\nrightElbow\nleftWrist\nrightWrist\nleftHip\nrightHip\nleftKnee\nrightKnee\nleftAnkle\nrightAnkle" > key_point_labels.txt

elif [[ ${model} == "person-detection-openvino" ]]; then
  mkdir -p openvino_models
  cd openvino_models
  download_url="https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/openvino/person_detection"
  wget ${download_url}/person-detection-retail-0013.xml
  wget ${download_url}/person-detection-retail-0013.bin

elif [[ ${model} == "face-detection-openvino" ]]; then
  mkdir -p openvino_models
  cd openvino_models
  download_url="https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/openvino/face_detection"
  wget ${download_url}/face-detection-retail-0005.xml
  wget ${download_url}/face-detection-retail-0005.bin

else
  echo -e "${model}:not a valid model_name\n"
  print_usage
fi
