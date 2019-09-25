# Android GUI NNStreamer Application Examples

## Prerequisite

We assume that you want to deploy a NNStreamer-based application on your own Android/ARM64bit target device.
Also, we assume that you already have experienced Android application developments with Android Studio.

 * Host PC:
   * OS: Ubuntu 16.04 x86_64 LTS
   * Android Studio: Ubuntu version (However, the Window version will be compatible.)
 * Target Device:
   * CPU Architecture: ARM 64bit (aarch64)
   * Android SDK: Min version 24 (Nougat)
   * Android NDK: Use default ndk-bundle in Android Studio
   * GStreamer: gstreamer-1.0-android-universal-1.16.0

## Build example

NNStreamer-SSD: Simple object detection example with TF-Lite model

We built a example using GStreamer tutorials and camera2 source for Android.
- [GStreamer tutorials](https://gitlab.freedesktop.org/gstreamer/gst-docs/)
- [Camera2 source for Android](https://justinjoy9to5.blogspot.com/2017/10/gstreamer-camera-2-source-for-android.html)

![ssd-example screenshot](screenshot/screenshot_ssd.jpg)

#### Setup Android Studio

To build a NNStreamer-based application, you should download Android Studio and setup environment variables.

Please see the details [here](https://github.com/nnsuite/nnstreamer/blob/master/api/android/README.md).

#### Download NNStreamer and example source code

```bash
$ cd $ANDROID_DEV_ROOT/workspace
$ git clone https://github.com/nnsuite/nnstreamer.git
$ git clone https://github.com/nnsuite/nnstreamer-example.git
```

Extract external libraries into common directory.

[extfiles.tar.xz](common/jni/extfiles.tar.xz) includes external library such as 'ahc'.

[tensorflow-lite-1.13.tar.xz](https://github.com/nnsuite/nnstreamer-android-resource/blob/master/android_api/ext-files/tensorflow-lite-1.13.tar.xz) includes the libraries and header files of tensorflow-lite.

```
$ cd $ANDROID_DEV_ROOT/workspace/nnstreamer-example/android/example_app/common/jni
$ tar xJf ./extfiles.tar.xz
$ svn --force export https://github.com/nnsuite/nnstreamer-android-resource/trunk/android_api/ext-files/tensorflow-lite-1.13.tar.xz
$ tar xJf ./tensorflow-lite-1.13.tar.xz # Check tensorflow-lite version and extract prebuilt library
$ ls ahc tensorflow-lite
```

#### Build the source code with Android Studio

Run Android Studio.

```bash
# If Android Studio was installed under the directory '/opt'
$ /opt/android-studio/bin/studio.sh
```

Import project in Android Studio.

![studio-import screenshot](screenshot/screenshot_studio_import_project.png)

Check a target SDK version (File - Project Structure)

![studio-setting-1 screenshot](screenshot/screenshot_studio_setting_1.png)

Change a default directory of SDK.
- Change SDK directory (File - Settings - Appearance & Behavior - System Settings - Android SDK - SDK Tools)
  - ```$ANDROID_DEV_ROOT/tools/sdk```

![studio-setting-2 screenshot](screenshot/screenshot_studio_setting_2.png)

Build project.

![studio-apk screenshot](screenshot/screenshot_studio_apk.png)

#### Run .apk file

Before running .apk file on your Android device, You must copy a SSD model and label file into your target device manually.

Make directory and copy SSD model and label files into the internal storage of your own Android target device.

You can download these files from [nnsuite testcases repository](https://github.com/nnsuite/testcases/tree/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco).

```
# You must put the below SSD network model files in the internal storage of your Android target device.
{INTERNAL_STORAGE}/nnstreamer/tflite_model/box_priors.txt
{INTERNAL_STORAGE}/nnstreamer/tflite_model/coco_labels_list.txt
{INTERNAL_STORAGE}/nnstreamer/tflite_model/ssd_mobilenet_v2_coco.tflite

```

## Terminology
* AHC: Android Hardware Camera2
* JNI: Java Native Interface
* SSD: Single Shot MultiBox Detector
* ABI: Application Binary Interface
* API: Application Programming Interface
* Cairo: A 2D graphics library with support for multiple output device
