## Ubuntu Native NNStreamer GUI Application - EZStreamer

### Introduction
EZStreamer is a native nnstreamer application on ubuntu environment. 

This example offers convenient **graphical user interface (GUI)** which is based on PyQT5 and PySide. 

### Features

1. The ability of showing basic camera video.
2. Eye Mask: The ability of covering detected eyes with black box. 
   - With 'Detect Main Streamer' option on, a red rectangle is drawn around the detected main streamer's face.
3. Face Mask: The ability of covering detected faces which is not the biggest. 
   - Faces are covered with a rectangle in a gray mosaic pattern.
   - With 'Detect Main Streamer' option on, main streamer's face is also detected and screened.
4. Object Detection: The ability of detecting objects in the video. 
5. 'Eye Mask' or 'Face Mask' function can be used with 'Object Detection' at the same time.
6. You can switch modes while running the application.
7. You can air on YouTube using RTMP protocol.
   - YouTube RTMP authorization key is required.

### How to Run

Since the example is based on `GLib` and `GObject`, these packages need to be installed before running. NumPy is also needed.
Additional packages for GUI application requires to run. To install PyQT5, check details below.

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
$ python3 ezstreamer.py
```



