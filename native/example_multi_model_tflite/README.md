## Ubuntu Native NNStreamer Application Example - Multi-Model

### Introduction

This examples passes camera video stream to two neural networks using **tensor_filters**. There are two examples for multi-model applications.
Both examples uses the neural network for face detection (detect_face.tflite) catches people's faces.

One example use neural network for object detection (ssd_mobilenet_v2_coco.tflite) to predicts the label of given image.

Another example use neural network for pose estimation (posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite) to predict body parts and reflect the results in images. In this example, two eyes are masked with a black box for each faces like a parasite the movie poster. Currently, up to 3 faces can be masked using this function, but 2 faces are recommended due to optimization issue.

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
$ ./get-model.sh pose-estimation-tflite
$ python nnstreamer_example_multi_model_tflite.py
$ python nnstreamer_example_multi_model_face_pose.py
```
