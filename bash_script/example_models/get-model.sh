#!/usr/bin/env bash

print_usage()
{
  echo -e "usage: $0 [model_name]"
  echo -e "model_name\timage-classification-tflite"
  echo -e "\t\timage-classification-caffe2"
  echo -e "\t\timage-classification-ncnn"
  echo -e "\t\tobject-detection-tf"
  echo -e "\t\tobject-detection-tflite"
  echo -e "\t\tobject-detection-ncnn"
  echo -e "\t\tface-detection-tflite"
  echo -e "\t\thand-detection-tflite"
  echo -e "\t\tspeech-command"
  echo -e "\t\timage-segmentation-tflite"
  echo -e "\t\ttext-classification-tflite"
  echo -e "\t\tpose-estimation-tflite"
  echo -e "\t\tperson-detection-openvino"
  echo -e "\t\tface-detection-openvino"
  echo -e "\t\tlow-light-image-enhancement"
  echo -e "\t\timage-style-transfer-onnx"
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

elif [[ ${model} == "image-classification-ncnn" ]]; then
  mkdir -p ncnn_model
  cd ncnn_model
  wget "https://github.com/Tencent/ncnn/raw/master/examples/synset_words.txt"
  mv synset_words.txt squeezenet_labels.txt
  download_url="https://github.com/nihui/ncnn-assets/raw/master/models"
  wget ${download_url}/squeezenet_v1.1.bin
  wget ${download_url}/squeezenet_v1.1.param

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

elif [[ ${model} == "object-detection-ncnn" ]]; then
  mkdir -p ncnn_model
  cd ncnn_model
  wget "https://github.com/openvinotoolkit/open_model_zoo/raw/master/data/dataset_classes/voc_20cl_bkgr.txt"
  mv voc_20cl_bkgr.txt voc_labels.txt
  download_url="https://github.com/nihui/ncnn-assets/raw/master/models"
  wget ${download_url}/mobilenetv2_ssdlite_voc.bin
  wget ${download_url}/mobilenetv2_ssdlite_voc.param

elif [[ ${model} == "face-detection-tflite" ]]; then
  mkdir -p tflite_model
  cd tflite_model
  download_url="http://ci.nnstreamer.ai/warehouse/nnmodels/"
  wget ${download_url}/detect_face.tflite
  wget ${download_url}/labels_face.txt
  wget ${download_url}/box_priors.txt

elif [[ ${model} == "hand-detection-tflite" ]]; then
  mkdir -p tflite_model
  cd tflite_model
  download_url="http://ci.nnstreamer.ai/warehouse/nnmodels/"
  wget ${download_url}/detect_hand.tflite
  wget ${download_url}/labels_hand.txt
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
  download_url="https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow-lite/pose_estimation"
  wget ${download_url}/posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite
  wget ${download_url}/point_labels.txt

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

elif [[ ${model} == "low-light-image-enhancement" ]]; then
  mkdir -p tflite_low_light_image_enhancement
  cd tflite_low_light_image_enhancement
  download_url="https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow-lite/zero_dce_tflite"
  wget ${download_url}/lite-model_zero-dce_1.tflite

elif [[ ${model} == "image-style-transfer-onnx" ]]; then
  mkdir -p onnx_image_style_transfer
  cd onnx_image_style_transfer
  download_url="https://raw.githubusercontent.com/microsoft/Windows-Machine-Learning/master/Samples/FNSCandyStyleTransfer/UWP/cs/Assets"
  wget ${download_url}/candy.onnx
  wget ${download_url}/la_muse.onnx
  wget ${download_url}/mosaic.onnx
  wget ${download_url}/udnie.onnx
  
else
  echo -e "${model}:not a valid model_name\n"
  print_usage
fi
