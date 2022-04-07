# NNStreamer Examples

This repository shows developers how to create their applications with nnstreamer/gstreamer. We recommend to install nnstreamer by downloading prebuilt binary packages from Launchpad/PPA (Ubuntu) or Download.Tizen.org (Tizen). If you want to build nnstreamer in your system for your example application builds, pdebuild (Ubuntu) with PPA or gbs (Tizen) are recommended for building nnstreamer. This repository has been detached from nnstreamer.git to build examples independently from the nnstreamer source code since Jan-09-2019.

Ubuntu PPA: nnstreamer/ppa [[PPA Main](https://launchpad.net/~nnstreamer/+archive/ubuntu/ppa)]<br />
Tizen Project: devel:Tizen:6.0:AI [[OBS Project](https://build.tizen.org/project/show/devel:Tizen:6.0:AI)] [[RPM Repo](http://download.tizen.org/live/devel%3A/Tizen%3A/6.0%3A/AI/Tizen_Unified_standard/)]


We provide example nnstreamer applications:

- Traditional Linux native applications
   - Linux/Ubuntu: GTK+ application
   - gst-launch-1.0 based scripts
- Tizen GUI Application
   - Tizen C/C++ application
   - Tizen .NET (C#) application
   - Tizen Web application
- Android applications
   - NDK based C/C++ CLI applicaton
   - JNI based GUI application


# Quick start guide for NNStreamer example

## Use PPA
* Download nnstreamer :
```
$ sudo add-apt-repository ppa:nnstreamer/ppa
$ sudo apt-get update
$ sudo apt-get install nnstreamer
$ sudo apt-get install nnstreamer-example
$ cd /usr/lib/nnstreamer/bin # binary install directory
```

 *note: `nnstreamer-example` ppa was integrated into `nnstreamer` ppa.
 
As of 2018/10/13, we support 16.04 and 18.04

If you want to build nnstreamer example yourself, please refer to the link : [[Build example](https://github.com/nnstreamer/nnstreamer/wiki/usage-examples-screenshots#build-examples-ubuntu-1604)]

## Usage Examples
### Text classification
```
$ cd /usr/lib/nnstreamer/bin
$ ./nnstreamer_example_text_classification_tflite
```

Refer to this link for more examples : [[NNStreamer example](https://github.com/nnstreamer/nnstreamer/wiki/usage-examples-screenshots#usage-examples)]

# Example List

## Ubuntu Native

| Application | Implementations | Used gstreamer/nnstreamer feature |
| -- | -- | -- |
| [![image-classification](./native/example_image_classification_tflite/image_classification_tflite_demo.webp)](./native/example_image_classification_tflite)<br> [Image Classification](./native/example_image_classification_tflite) | - [C (tflite)](./native/example_image_classification_tflite/nnstreamer_example_image_classification_tflite.c)<br> - [Python (tflite)](./native/example_image_classification_tflite/nnstreamer_example_image_classification_tflite.py)<br> - [C (ONE)](./native/example_image_classification_nnfw/nnstreamer_example_image_classification_nnfw.c)<br> - [C (caffe2)](./native/example_image_classification_caffe2/nnstreamer_example_image_classification_caffe2.c) | - v4l2src for input image stream<br> - textoverlay for showing labels |
| [![object-detection](./native/example_object_detection_tensorflow_lite/object_detection_tflite_demo.webp)](./native/example_object_detection_tensorflow_lite)<br> [Object Detection](./native/example_object_detection_tensorflow_lite) | - [C++ (tf)](./native/example_object_detection_tensorflow/nnstreamer_example_object_detection_tf.cc)<br> - [C++ (tflite)](./native/example_object_detection_tensorflow_lite/nnstreamer_example_object_detection_tflite.cc)<br> - [Python (tflite)](./native/example_object_detection_tensorflow_lite/nnstreamer_example_object_detection_tflite.py) | - v4l2src for input image stream<br> - cairooverlay for drawing boxes |
| [![pose-estimation](./native/example_pose_estimation_tflite/yongjoo2.webp)](./native/example_pose_estimation_tflite)<br> [Pose Estimation](./native/example_pose_estimation_tflite) | - [C++ (tflite)](./native/example_pose_estimation_tflite/nnstreamer_example_pose_estimation_tflite.cc)<br> - [Python (tflite)](./native/example_pose_estimation_tflite/nnstreamer_example_pose_estimation_tflite.py) | - v4l2src for input image stream<br> - cairooverlay for drawing body points |
| Image Classification with tensor_decoder | - [C++ (tflite)](./native/example_decoder_image_labelling/nnstreamer_example_decoder_image_labelling.c) | - [`tensor_decoder mode=image_labeling`](https://github.com/nnstreamer/nnstreamer/blob/main/ext/nnstreamer/tensor_decoder/tensordec-imagelabel.c) for postprocessing<br> | 9/P | TBD |
| Text Classification| - [C (tflite)](./native/example_text_classification/nnstreamer_example_text_classification_tflite.c) | - appsrc for input text data |
| [![object-detection-2cam](./native/example_object_detection_tflite_2cam/phone.webp)](./native/example_object_detection_tflite_2cam)<br> [Object Detection with 2 cameras](./native/example_object_detection_tflite_2cam) | - [C++ (tflite)](./native/example_object_detection_tflite_2cam/nnstreamer_example_object_detection_tflite_2cam.cc) | - v4l2src for input image stream<br> - cairooverlay for drawing boxes |

## With gst-launch-1.0

| Application | Implementations | Used gstreamer/nnstreamer feature |
| -- | -- | -- |
| [Object Detection](./bash_script/example_object_detection_tensorflow_lite/gst-launch-object-detection-tflite.sh) | - [tflite](./bash_script/example_object_detection_tensorflow_lite/gst-launch-object-detection-tflite.sh) <br> - [tf](./bash_script/example_object_detection_tensorflow/gst-launch-object-detection-tf.sh) | - v4l2src for input image stream<br> - [`tensor_decoder mode=bounding_boxes`](https://github.com/nnstreamer/nnstreamer/blob/main/ext/nnstreamer/tensor_decoder/tensordec-boundingbox.c) for postprocessing<br> - compositor for drawing decoded boxes |
| [Image Segmentation](./bash_script/example_image_segmentation_tensorflow_lite) | - [tflite](./bash_script/example_image_segmentation_tensorflow_lite/gst-launch-image-segmentation-tflite.sh)<br> - [Edge TPU](./bash_script/example_image_segmentation_tensorflow_lite)<br> &nbsp; &nbsp; * [Edge-AI server](./bash_script/example_image_segmentation_tensorflow_lite/gst-launch-image-seg-flatbuf-edgetpu-server.sh)<br> &nbsp; &nbsp; * [Edge-AI client](./bash_script/example_image_segmentation_tensorflow_lite/gst-launch-image-seg-flatbuf-edgetpu-client.sh)<br> | - v4l2src for input image stream<br> - [`tensor_decoder mode=image_segment`](https://github.com/nnstreamer/nnstreamer/blob/main/ext/nnstreamer/tensor_decoder/tensordec-imagesegment.c) for postprocessing<br>- tcpclientsrc / tcpserversink / gdppay / gdpdepay for networking between devices<br> - `tensor_converter` / `tensor_decoder mode=flatbuf` for using Flatbuffers  |
| [Pipeline Flow Control in Face Detection](./bash_script/example_tensorif) | - [OpenVINO + tflite + passthrough](./bash_script/example_tensorif/gst-launch-tensorif-passthrough.sh)<br> - [OpenVINO + tflite + tensorpick](./bash_script/example_tensorif/gst-launch-tensorif-tensorpick.sh) | - v4l2src for input image stream<br> - `tensor_if` for flow control |

## Tizen Applications

| Application | Implementations | Used gstreamer/nnstreamer feature |
| -- | -- | -- |
| [<img src="./Tizen.native/ImageClassification_SingleShot/screenshot.png" width="250">](./Tizen.native/ImageClassification)<br> [Image Classification](./Tizen.native/ImageClassification) | - [Native App with Pipeline API](./Tizen.native/ImageClassification) <br> - [Native App with Single API](./Tizen.native/ImageClassification_SingleShot) | - appsrc for input image data from hardward camera<br> - [Machine Learning Inference Native API](https://docs.tizen.org/application/native/guides/machine-learning/machine-learning-inference/) |
| [![object-detection](./Tizen.native/ObjectDetection/tizen-objectdetection-demo.webp)](./Tizen.native/ObjectDetection)<br> [Object Detection](./Tizen.native/ObjectDetection) | - [Native App with Pipeline API](Tizen.native/ObjectDetection) | - appsrc for input image data from hardward camera<br> - [`tensor_decoder mode=bounding_boxes`](https://github.com/nnstreamer/nnstreamer/blob/main/ext/nnstreamer/tensor_decoder/tensordec-boundingbox.c) for postprocessing<br> - [Pipeline API](https://docs.tizen.org/application/native/guides/machine-learning/machine-learning-inference/#pipeline-api) |
| [![text-classification](./Tizen.NET/TextClassification/text-classification-demo.webp)](./Tizen.NET/TextClassification)<br>[Text Classification](./Tizen.NET/TextClassification) | - [Native App with Pipeline API](./Tizen.native/TextClassification)<br>- [.NET App with C# API](./Tizen.NET/TextClassification) | - appsrc for input text data<br>- [Pipeline API](https://docs.tizen.org/application/native/guides/machine-learning/machine-learning-inference/#pipeline-api)<br> - [C# API](https://samsung.github.io/TizenFX/API8/api/Tizen.MachineLearning.Inference.html) |
| [![orientation-detection](./Tizen.native/OrientationDetection/orientation_detection.webp)](./Tizen.native/OrientationDetection)<br>[Orientation Detection](./Tizen.native/OrientationDetection) | - [Native App with Pipeline API](./Tizen.native/OrientationDetection)<br>- [.NET App with C# API](./Tizen.NET/OrientationDetection) | - [`tensor_src_tizensensor type=accelerometer`](https://github.com/nnstreamer/nnstreamer/blob/main/ext/nnstreamer/tensor_source/tensor_src_tizensensor.c)  for hardware sensor data<br>- [Pipeline API](https://docs.tizen.org/application/native/guides/machine-learning/machine-learning-inference/#pipeline-api)<br> - [C# API](https://samsung.github.io/TizenFX/API8/api/Tizen.MachineLearning.Inference.html) |
| [![face_landmark](./Tizen.platform/Tizen_IoT_face_landmark/face_landmark.webp)](./Tizen.platform/Tizen_IoT_face_landmark)<br>[Face Landmark](./Tizen.platform/Tizen_IoT_face_landmark) | - [Tizen IoT App](./Tizen.platform/Tizen_IoT_face_landmark) | - v4l2src for input data from camera<br>- tizenwlsink for lendering video to display<br>- cairooverlay for drawing dots |

## Android Applications

| Application | Implementations | Used gstreamer/nnstreamer feature |
| -- | -- | -- |
| [<img src="android/example_app/screenshot/screenshot_ssd.jpg" width="300">](./android/example_app/nnstreamer-ssd)<br> [Object Detection](./android/example_app/nnstreamer-ssd) | - [w/ JNI + ahc2src](./android/example_app/nnstreamer-ssd)<br> - [w/ JNI + amcsrc](./android/example_app/nnstreamer-media-ssd) | - [`ahc2src`](https://justinjoy9to5.blogspot.com/2017/10/gstreamer-camera-2-source-for-android.html) for hardware camera input<br> - [`amcsrc`](https://github.com/nnstreamer/nnstreamer/tree/main/ext/nnstreamer/android_source) for media file input |
| [<img src="https://play-lh.googleusercontent.com/3piil-gS4-3NgTXdSdJS-YxvC4seFnuzq3bTWHap5Z7n2dc06OEUbT-zL2y6HJRHtA=w1861-h949-rw" width="300">](https://play.google.com/store/apps/details?id=org.freedesktop.gstreamer.nnstreamer.multi)<br> [NNStreamer Multi Model<br>Face Detection + Hand Detection +<br> Object Detection + Pose Estimation](https://play.google.com/store/apps/details?id=org.freedesktop.gstreamer.nnstreamer.multi) | - [JNI App](android/example_app/nnstreamer-multi)<br> - [GooglePlay App](https://play.google.com/store/apps/details?id=org.freedesktop.gstreamer.nnstreamer.multi) | - [`ahc2src`](https://justinjoy9to5.blogspot.com/2017/10/gstreamer-camera-2-source-for-android.html) for hardware camera input<br> - [application code](./android/example_app/nnstreamer-multi/jni/nnstreamer-ex.cpp) for multiple pipeline control  |
| [![camera-java-api](./android/example_app/use-camera-with-nnstreamer-java-api/camera_nnstreamer_screenshot.webp)](./android/example_app/use-camera-with-nnstreamer-java-api)<br> [Sample App using NNStreamer JAVA API](./android/example_app/use-camera-with-nnstreamer-java-api) | - [JAVA w/o JNI](./android/example_app/use-camera-with-nnstreamer-java-api/src/main/java/org/nnsuite/nnstreamer/sample/MainActivity.java) | - `appsrc` for input image data from hardward camera<br> - `videoflip` for flipping image<br> - [NNStreamer JAVA API](https://github.com/nnstreamer/nnstreamer/tree/main/api/android/api/src/main/java/org/nnsuite/nnstreamer) |
