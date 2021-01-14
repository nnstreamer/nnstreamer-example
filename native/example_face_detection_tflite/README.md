---
title: Face detection
...

## Ubuntu Native NNStreamer Application Example - Face Detection
### Introduction

This example passes camera video stream to a neural network using **tensor_filter**. The neural network detects faces of people in input stream. 

If there are multiple faces in the video, detected faces which are not the largest one are pixelated.

The application takes advantage of **cairooverlay** GStreamer plugin.

#### How to Run

Since the example is based on `GLib` and `GObject`, these packages need to be installed before running. NumPy is also needed.

```bash
$ sudo apt-get install pkg-config libcairo2-dev gcc python3-dev libgirepository1.0-dev
$ pip3 install gobject PyGObject numpy
```

This example requires specific tflite model and label data.

**get-model.sh** download these resources.

```bash
# bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh face-detection-tflite
$ python nnstreamer_example_face_detection_tflite.py
```



