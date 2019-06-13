#!/usr/bin/env bash
mkdir -p tf_model
cd tf_model
wget https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow/ssdlite_mobilenet_v2/ssdlite_mobilenet_v2.pb
wget https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow/ssdlite_mobilenet_v2/coco_labels_list.txt
