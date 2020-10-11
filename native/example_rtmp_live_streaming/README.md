## Ubuntu Native NNStreamer Application Example - RTMP Streaming and GUI Application (EZ Streamer)

### Introduction
This example use rtmpsink to broadcast video streaming. This examples uses Youtube as live streaming platform.
nnstreamer_example_face_detection_rtmp.py is same as face_detection_tflite, but with rtmp support. And this example includes GUI Application that using PyQT5 and PySide.
All of examples requires youtube rtmp auth key.

More Details will be updated until 2020-10-19.

#### How to Run

Since the example is based on `GLib` and `GObject`, these packages need to be installed before running. NumPy is also needed.
Additional packages for gui application requires to run. To install PyQT5, check details below.

```bash
$ sudo apt-get install pkg-config libcairo2-dev gcc python3-dev libgirepository1.0-dev python3-numpy
$ sudo apt-get install python3-pyqt5 qttools5-dev-tools python3-pyqt5.qtmultimedia \
                    python3-pyside python3-pyside.qtcore python3-pyside.qtgui python3-pyside.qtuitools
$ pip3 install gobject PyGObject
```

This example requires specific tflite models and label data.

**get-model.sh** download these resources.

```bash
# bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh face-detection-tflite
$ ./get-model.sh object-detection-tflite
$ ./get-model.sh pose-estimation-tflite
$ python3 nnstreamer_example_face_detection_rmtp.py <youtube rtmp auth key>
$ python3 ezstreamer.py
```
