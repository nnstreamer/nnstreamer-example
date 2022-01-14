---
title: Low Light Image Enhancement (tflite)
...

## Ubuntu Native NNStreamer Application Example - Low Light Image Enhancement
### Introduction
This example passes a png image to a neural network using **tensor_filter**.
Then the neural network makes enhanced brightness of that image.

### How to Run
This example requires specific tflite model.
The public model can be obtained from this link: https://tfhub.dev/sayannath/lite-model/zero-dce/1 <br>
**get-model.<span>sh** download these resources.
```bash
# bash
# python
$ ./get-model.sh low-light-image-enhancement
$ python3 nnstreamer_example_low_light_image_enhancement.py

# cc
$ cd $NNST_ROOT/bin
$ ./get-model.sh low-light-image-enhancement
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:$NNST_ROOT/lib/gstreamer-1.0
$ ./nnstreamer_example_low_light_image_enhancement
```

### Screenshot of Result
The public image can be obtained from this link : https://paperswithcode.com/dataset/lol<br><br>
![Alt original](./original.png)<br>
original image<br><br>
![Alt enhancement](./enhancement.png)<br>
enahncement image
