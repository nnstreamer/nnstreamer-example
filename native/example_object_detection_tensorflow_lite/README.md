---
title: Object Detection (tflite)
...

## Ubuntu Native NNStreamer Application Example - Object Detection
This example passes camera video stream to a neural network using **tensor_filter**. 
Then the given neural network predicts multiple objects with bounding boxes. The detected boxes are drawen by **cairooveray** GStreamer plugin.

### How to Run
This example requires tflite model, labels and box priors.  
The resources can be obtained from this link: https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco   
**get-model.<span>sh** download the required model and other txt data files.
```bash
# bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh object-detection-tflite
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:$NNST_ROOT/lib/gstreamer-1.0
$ ./nnstreamer_example_object_detection_tflite
```

### Screenshots
![Alt me](./object_detection_tflite_demo.webp)
