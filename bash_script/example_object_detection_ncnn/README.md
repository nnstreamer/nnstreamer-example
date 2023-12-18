## Ubuntu Native NNStreamer Application Example - Object Detection
### Introduction
This example passes camera video stream to a neural network using **tensor_filter**. 
Then the neural network draws **bounding boxes** around several objects by **tensor_decoder**.

### How to Run
This example requires specific ncnn model and label data (mobilenetv2_ssdlite_voc).  
You can download param file and bin file from this link: https://github.com/nihui/ncnn-assets/tree/master/models/
Adjust the model file path appropriately.
