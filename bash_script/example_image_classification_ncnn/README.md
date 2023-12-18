## Ubuntu Native NNStreamer Application Example - Image Classification
### Introduction
This example passes camera video stream to a neural network using **tensor_filter**. 
Then the neural network predicts the label of given image. The results are drawen by **textoverlay** GStreamer plugin.

### How to Run
This example requires specific ncnn model and label data (squeezenet_v1.1).  
You can download param file and bin file from this link: https://github.com/nihui/ncnn-assets/tree/master/models/
Adjust the model file path appropriately.
