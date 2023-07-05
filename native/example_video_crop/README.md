---
title: Video Crop (tflite)
...

## Ubuntu Native NNStreamer Application Example - Video Crop
This example passes input video stream to a neural network using **tensor_filter**. 
Then the given neural network predicts multiple objects. The probability score of those object along with their bounding box info is passed to **tensor_decoder::tensor_region** subplugin which passes the cropping info of top n (set by pipeline developer) detected objects to **tensor_crop** which crops the video.

### How to Run
This example requires tflite model, labels and box priors.  
The resources can be obtained from this link: https://github.com/nnsuite/testcases/raw/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco   
**get-model.<span>sh** download the required model and other txt data files.
```bash
# bash
$ cd $NNST_ROOT/bin
$ ./get-model.sh object-detection-tflite
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:$NNST_ROOT/lib/gstreamer-1.0
$ ./nnstreamer_example_video_crop_tflite
```
