# Android GUI NNStreamer Application Examples

## Prerequisite

We assume that you want to deploy a NNStreamer-based application on your own Android/ARM64bit target device.
Also, We assume that you already have experienced Android application developments with Android Studio.

 * Host PC:
   * OS: Ubuntu 16.04 x86_64 LTS
   * Android Studio: Ubuntu version (However, the Window version will be compatible.)
 * Target Device:
   * CPU Architecture: ARM 64bit (aarch64)
   * Android Platform: 7.0 (Nougat)
   * Android NDK: r15c
   * Android API level: 24
   * Gstreamer: gstreamer-1.0-android-universal-1.16.0

## Build example

NNStreamer-SSD: Simple object detection example with TF-Lite model

We built a example using GStreamer tutorials and camera2 source for Android.
- [GStreamer tutorials](https://gitlab.freedesktop.org/gstreamer/gst-docs/)
- [Camera2 source for Android](https://justinjoy9to5.blogspot.com/2017/10/gstreamer-camera-2-source-for-android.html)

![ssd-example screenshot](screenshot/screenshot_ssd.jpg)

#### Environment variables

First of all, you need to set-up the development environment as following:
```bash
$ mkdir -p $HOME/android/tools/sdk
$ mkdir -p $HOME/android/tools/ndk
$ mkdir -p $HOME/android/gstreamer-1.0
$ mkdir -p $HOME/android/workspace
$
$ vi ~/.bashrc
# Environmet variables for developing a NNStreamer application
# $ANDROID_DEV_ROOT/gstreamer-1.0                # GStreamer binaries
# $ANDROID_DEV_ROOT/tools/ndk                    # Android NDK root directory
# $ANDROID_DEV_ROOT/tools/sdk                    # Android SDK root directory (default location: $HOME/Android/Sdk)
# $ANDROID_DEV_ROOT/workspace/nnstreamer         # nnstreamer cloned git repository
# $ANDROID_DEV_ROOT/workspace/nnstreamer-example # nnstreamer-example cloned git repository
#
export JAVA_HOME=/opt/android-studio/jre            # JRE path in Android Studio
export ANDROID_DEV_ROOT=$HOME/android               # Set your own path (The default path will be "$HOME/Android".)
export ANDROID_SDK=$ANDROID_DEV_ROOT/tools/sdk      # Android SDK (The default path will be "$HOME/Android/Sdk".)
export ANDROID_NDK=$ANDROID_DEV_ROOT/tools/ndk      # Android NDK (Revision 15c)
export GSTREAMER_ROOT_ANDROID=$ANDROID_DEV_ROOT/gstreamer-1.0
export PATH=$PATH:$ANDROID_NDK:$ANDROID_SDK/platform-tools:$JAVA_HOME/bin
export NNSTREAMER_ROOT=$ANDROID_DEV_ROOT/workspace/nnstreamer
```

#### Download Android Studio

Download and install Android Studio to compile an Android source code.
You can see the installation guide [here](https://developer.android.com/studio/install).

For example,
```bash
$ firefox  https://developer.android.com/studio
Then, download "Android Studio" in the /opt folder.
$ cd /opt
$ wget https://dl.google.com/dl/android/studio/ide-zips/3.4.0.18/android-studio-ide-183.5452501-linux.tar.gz
$ tar xvzf ./android-studio-ide-183.5452501-linux.tar.gz
```

#### Download NDK (Revision 15c)

Note that you must download and decompress ```Android NDK, Revision 15c``` to compile normally a GStreamer-based plugin (e.g., NNStreamer).

You can download older version from [here](https://developer.android.com/ndk/downloads/older_releases.html).

For example,
```bash
$ mkdir -p $ANDROID_DEV_ROOT/tools
$ cd  $ANDROID_DEV_ROOT/tools
$ wget https://dl.google.com/android/repository/android-ndk-r15c-linux-x86_64.zip
$ unzip android-ndk-r15c-linux-x86_64.zip
$ mv android-ndk-r15c-linux-x86_64 ndk
```

#### Download GStreamer binaries

You can get the prebuilt GStreamer binaries from [here](https://gstreamer.freedesktop.org/data/pkg/android/).

For example,
```bash
$ cd $ANDROID_DEV_ROOT/
$ wget https://gstreamer.freedesktop.org/data/pkg/android/1.16.0/gstreamer-1.0-android-universal-1.16.0.tar.xz
$ mkdir gstreamer-1.0
$ cd gstreamer-1.0
$ tar xJf gstreamer-1.0-android-universal-1.16.0.tar.xz
```

Download nnStreamer and nnstreamer-example source code.

```bash
$ cd $ANDROID_DEV_ROOT/workspace
$ git clone https://github.com/nnsuite/nnstreamer.git
$ git clone https://github.com/nnsuite/nnstreamer-example.git
```

Extract external libraries in jni directory.

[extlibs.tar.xz](nnstreamer-ssd/jni/extlibs.tar.xz) includes two directories such as 'ahc' and 'tensorflow-lite'.
The directories includes the library and header files to run the pipeline based on NNStreamer.

```
$ cd $ANDROID_DEV_ROOT/workspace/nnstreamer-example/android/example_app/nnstreamer-ssd/jni
$ tar xJf ./extlibs.tar.xz
$ ls ahc tensorflow-lite
```

You must remove ```-nostdlib++``` option in the gstreamer-1.0.mk file to prevent build error generated in a linking step of GStreamer.

```
# Remove '-nostdlib++' in GSTREAMER_ANDROID_CMD of the gstreamer-1.0.mk file for the ARM-based Android software stack.
$ vi $GSTREAMER_ROOT_ANDROID/{armv7|arm64}/share/gst-android/ndk-build/gstreamer-1.0.mk
GSTREAMER_ANDROID_CMD := $(call libtool-link,$(TARGET_CXX) $(GLOBAL_LDFLAGS) $(TARGET_LDFLAGS) -shared ...
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

Change a default directory of NDK and SDK.
- Change SDK directory (File - Settings - Appearance & Behavior - System Settings - Android SDK - SDK Tools)
  - ```$ANDROID_DEV_ROOT/tools/sdk```
- Change NDK directory (File - Project Structure - SDK Location)
  - ```$ANDROID_DEV_ROOT/tools/ndk```

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
