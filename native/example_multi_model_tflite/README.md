## Ubuntu Native NNStreamer Application Example - Multi-Model

### Introduction

This example passes camera video stream to two neural networks using **tensor_filters**. Then the neural network for face detection (detect_face.tflite) catches people's faces and the neural network for object detection (ssd_mobilenet_v2_coco.tflite) predicts the label of given image.

The results are drawn by **cairooverlay** GStreamer plugin.

#### How to Run

Since the example is based on `GLib` and `GObject`, these packages need to be installed before running. NumPy is also needed.

```bash
$ sudo apt-get install pkg-config libcairo2-dev gcc python3-dev libgirepository1.0-dev
$ pip3 install gobject PyGObject numpy
```

This example requires specific tflite models and label data.

**get-model.sh** download these resources.

```bash
# bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh face-detection-tflite
$ ./get-model.sh object-detection-tflite
$ python nnstreamer_example_multi_model_tflite.py
```




