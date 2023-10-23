#!/usr/bin/env bash

# Get the yamnet/classification tflite model file
wget https://tfhub.dev/google/lite-model/yamnet/classification/tflite/1?lite-format=tflite -O yamnet_classification.tflite

# Unzip the label file "yamnet_label_list.txt" from the model file
unzip yamnet_classification.tflite

# Run gstreamer pipeline.
# Simple explanation:
#   - get audio from some device (webcam or mic. You should check the device number)
#   - its sample rate is 16000 hz
#   - make 15600-length tensor hopping 3900 sample
#   - 32768 ~ +32767 -> -1.0 ~ +1.0
#   - feed the input to the model
#   - get argmax of the output and get the corresponding label. note that the confidence is not considered.
#   - display the label via textoverlay
gst-launch-1.0 \
  alsasrc ! audioconvert ! audio/x-raw,format=S16LE,channels=1,rate=16000,layout=interleaved ! \
    tensor_converter frames-per-tensor=3900 ! \
    tensor_aggregator frames-in=3900 frames-out=15600 frames-flush=3900 frames-dim=1 ! \
    tensor_transform mode=arithmetic option=typecast:float32,add:0.5,div:32767.5 ! \
    tensor_transform mode=transpose option=1:0:2:3 ! \
    queue leaky=2 max-size-buffers=10 ! tensor_filter framework=tensorflow2-lite model=yamnet_classification.tflite custom=Delegate:XNNPACK,NumThreads:4 ! \
    tensor_decoder mode=image_labeling option1=yamnet_label_list.txt ! overlay.text_sink \
  videotestsrc ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB ! queue ! overlay.video_sink \
  textoverlay name=overlay font-desc="Sans, 24" ! videoconvert ! ximagesink sync=false
