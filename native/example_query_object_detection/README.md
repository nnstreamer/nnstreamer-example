---
title: query object detection
...

## Ubuntu Native NNStreamer Application Example - Object Detection using tensor query
### Introduction
The client sends a video frame to the server, then the server performs object detection (which requires high-performance work) and sends the result to the client.

#### How to Run
This example requires specific tensorflow-lite model and label data.
**get-model.sh** download these resources.
```bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh object-detection-tflite
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:$NNST_ROOT/lib/gstreamer-1.0
# Run the server
$ ./nnstreamer_example_query_object_detection server
# Run the client on another shell.
$ ./nnstreamer_example_query_object_detection client
```
