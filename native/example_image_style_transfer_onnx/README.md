---
title: Image Style Transfer (onnx)
...

## Ubuntu Native NNStreamer Application Example - Image Style Transfer
### Introduction
This example passes camera video stream to a neural network using **tensor_filter**. 

### How to Run
This example requires specific onnx models.  
The public model can be obtained from this link: https://github.com/onnx/models/blob/main/validated/vision/style_transfer/fast_neural_style/README.md
**get-model.<span>sh** download these resources.
```bash
# bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh image_style_transfer_onnx
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:$NNST_ROOT/lib/gstreamer-1.0
$ ./nnstreamer_example_image_style_transfer_onnx
```
### Screenshots
![Alt me](./onnx-style-transfer.webp)
