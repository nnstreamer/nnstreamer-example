#!/usr/bin/env bash
mkdir tflite_model_img
cd tflite_model_img
wget https://storage.googleapis.com/download.tensorflow.org/models/tflite/mobilenet_v1_1.0_224_quant_and_labels.zip 
unzip mobilenet_v1_1.0_224_quant_and_labels.zip
rm mobilenet_v1_1.0_224_quant_and_labels.zip
mv labels_mobilenet_quant_v1_224.txt labels.txt
