#!/usr/bin/env bash
mkdir speech_model
cd speech_model
wget https://storage.googleapis.com/download.tensorflow.org/models/tflite/conv_actions_tflite.zip
unzip conv_actions_tflite.zip
rm conv_actions_tflite.zip
