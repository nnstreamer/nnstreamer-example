---
title: Image Classification (tflite)
...

## Ubuntu Native NNStreamer Application Example - Image Classification
### Introduction
This example passes camera video stream to a neural network using **tensor_filter**. 
Then the neural network predicts the label of given image. The results are drawen by **textoverlay** GStreamer plugin.

### How to Run
This example requires specific tflite model and label data.  
The public model can be obtained from this link: https://storage.googleapis.com/download.tensorflow.org/models/tflite/mobilenet_v1_1.0_224_quant_and_labels.zip   
**get-model.<span>sh** download these resources.
```bash
# bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh image-classification-tflite
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:$NNST_ROOT/lib/gstreamer-1.0
$ ./nnstreamer_example_image_classification_tflite
```

### Screenshots
![Alt me](./image_classification_tflite_demo.webp)
