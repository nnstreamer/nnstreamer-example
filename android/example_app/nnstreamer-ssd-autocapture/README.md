# Android NNStreamer Application - AutoCapture

## Introduction

A camera application can auto capture with **object detecting**. Users can customize the condition of taking a photo. After setting conditions, when the picture meet conditions, the camera application take the picture automatically.



## How to Run

#### Build `AutoCapture` application

Please see [this guide](https://github.com/nnstreamer/nnstreamer-example/blob/master/android/example_app/README.md).

Follow until building project.



#### Install and Run Autocapture App

You can install your *NNStreamer App* on your Android device using Android Studio or **adb** command as below.

```
$ cd $ANDROID_DEV_ROOT/workspace/nnstreamer-example/android/example_app/nnstreamer-ssd-autocapture
$ cd build/outputs/apk/debug         # Build as debug mode
$ adb install nnstreamer-ssd-autocapture-debug.apk
```

When first launching `Autocapture app` on your Android device, Application automatically downloads a SSD-autocapture model and label file into your target device.

If your device does not access the Internet, you can download these files from [Our model repository](http://nnstreamer.mooo.com/warehouse/nnmodels/) on your PC and put them in the internal storage of your Android target device as below.

```
{INTERNAL_STORAGE}/nnstreamer/tflite_model/box_priors.txt
{INTERNAL_STORAGE}/nnstreamer/tflite_model/coco_labels_list.txt
{INTERNAL_STORAGE}/nnstreamer/tflite_model/ssd_mobilenet_v2_coco.tflite
...
```



## Usage Examples

#### Main screen

- AutoCapture Off / On

  ​                       <img src="res\screenshot\screenshot_autocapture-off.jpg" alt="image-autocapture-off" style="zoom: 67%;" />                       <img src="res\screenshot\screenshot_autocapture-on.jpg" alt="image-autocapture-on" style="zoom:67%;" />

Left button is for going to the gallery / Middle button is capture button / Right button is for going to setting screen.

Users can use autocapture function after turning on autocapture button. If users turn on autocapture function, the autocapture camera takes the picture when the picture meets users condition.



#### Setting screen

<img src="res\screenshot\screenshot_setting.jpg" alt="image-setting" style="zoom:67%;" />

This page is for setting conditions. Users can add a condition on input information box, and then a condition is added on settings box.



#### Preview screen

<img src="res\screenshot\screenshot_preview.jpg" alt="image-preview" style="zoom:67%;" />

After taking a picture, users can check the picture on this page. Users can save it or take a picture again.