# NNStreamer Examples

This repository shows developers how to create their applications with nnstreamer/gstreamer. We recommend to install nnstreamer by downloading prebuilt binary packages from Launchpad/PPA (Ubuntu) or Download.Tizen.org (Tizen). If you want to build nnstreamer in your system for your example application builds, pdebuild (Ubuntu) with PPA or gbs (Tizen) are recommended for building nnstreamer. This repository has been detached from nnstreamer.git to build examples independently from the nnstreamer source code since Jan-09-2019.

Ubuntu PPA: nnstreamer/ppa [[PPA Main](https://launchpad.net/~nnstreamer/+archive/ubuntu/ppa)]<br />
Tizen Project: devel:AIC:Tizen:5.0:nnsuite [[OBS Project](https://build.tizen.org/project/show/devel:AIC:Tizen:5.0:nnsuite)] [[RPM Repo](http://download.tizen.org/live/devel%3A/AIC%3A/Tizen%3A/5.0%3A/nnsuite/standard/)]


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
```

* Download nnstreamer example :
```
$ sudo add-apt-repository ppa:nnstreamer-example/ppa
$ sudo apt-get update
$ sudo apt-get install nnstreamer-example
$ cd /usr/lib/nnstreamer/bin # binary install directory
```

As of 2018/10/13, we support 16.04 and 18.04

If you want to build nnstreamer example yourself, please refer to the link : [[Build example](https://github.com/nnstreamer/nnstreamer/wiki/usage-examples-screenshots#build-examples-ubuntu-1604)]

## Usage Examples
### Text classification
```
$ cd /usr/lib/nnstreamer/bin
$ ./nnstreamer_example_text_classification_tflite
```

Refer to this link for more examples : [[NNStreamer example](https://github.com/nnstreamer/nnstreamer/wiki/usage-examples-screenshots#usage-examples)]
