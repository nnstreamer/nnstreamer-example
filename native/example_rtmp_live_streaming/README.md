## Ubuntu Native NNStreamer Application Example - RTMP Streaming

### Introduction

This example use **rtmpsink** to broadcast video streaming. This examples uses YouTube as live streaming platform. YouTube RTMP authorization key is required.

The main function of this example is covering detected faces which is not the biggest in the video with rectangles in gray mosaic patterns. It passes camera video stream to a neural network using **tensor_filter**. The neural network detects faces of people in input stream. 

The application uses **cairooverlay** GStreamer plugin.

### How to Run

Since the example is based on `GLib` and `GObject`, these packages need to be installed before running. NumPy is also needed.
Additional packages for gui application requires to run. To install PyQT5, check details below.

```bash
$ sudo apt-get install pkg-config libcairo2-dev gcc python3-dev libgirepository1.0-dev python3-numpy
$ pip3 install gobject PyGObject
```

This example requires specific tflite models and label data.

**get-model.sh** download these resources.

```bash
# bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh face-detection-tflite
$ python3 nnstreamer_example_face_detection_rmtp.py <youtube rtmp auth key>
```


