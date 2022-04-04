---
title: Palm Tracking
...

## Ubuntu Native NNStreamer Application Example - Palm Tracking
### Introduction
This example passes camera video stream to a neural network using **tensor_filter**.
The neural network detects plam positions from input stream. The results are drawn by **cairooverlay** GStreamer plugin.

### How to Run
This example requires palm detection tf lite model.  
The public model can be obtained from this link: https://github.com/google/mediapipe/raw/master/mediapipe/modules/palm_detection/palm_detection_lite.tflite
**get-model.<span>sh** download the required model and make label text file
```bash
# bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh palm-detection-lite
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:$NNST_ROOT/lib/gstreamer-1.0
$ ./nnstreamer_example_palm_detection_tflite
```

### Screenshots
![Alt palm](./palm_tracking.webp)
