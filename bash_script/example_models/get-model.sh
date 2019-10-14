#!/usr/bin/env bash


print_usage()
{
  echo "usage: $0 [model_name]"
  echo "model_name     image-classification-tflite"
  echo "               object-detection-tf"
  echo "               object-detection-tflite"
  echo "               speech-command"
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



if [ ${model} == image-classification-tflite ]; then
  mkdir -p tflite_model_img
  cd tflite_model_img
  wget https://storage.googleapis.com/download.tensorflow.org/models/tflite/mobilenet_v1_1.0_224_quant_and_labels.zip
  unzip mobilenet_v1_1.0_224_quant_and_labels.zip
  rm mobilenet_v1_1.0_224_quant_and_labels.zip
  mv labels_mobilenet_quant_v1_224.txt labels.txt

elif [ ${model} == object-detection-tf ]; then
  mkdir -p tf_model
  cd tf_model
  wget https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow/ssdlite_mobilenet_v2/ssdlite_mobilenet_v2.pb
  wget https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow/ssdlite_mobilenet_v2/coco_labels_list.txt

elif [ ${model} == object-detection-tflite ]; then
  mkdir -p tflite_model
  cd tflite_model
  wget https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco/ssd_mobilenet_v2_coco.tflite
  wget https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco/coco_labels_list.txt
  wget https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco/box_priors.txt

elif [ ${model} == speech-command ]; then
  mkdir -p speech_model
  cd speech_model
  wget https://storage.googleapis.com/download.tensorflow.org/models/tflite/conv_actions_tflite.zip
  unzip conv_actions_tflite.zip
  rm conv_actions_tflite.zip

else
  echo -e "${model}:not a valid model_name\n"
  print_usage
fi
