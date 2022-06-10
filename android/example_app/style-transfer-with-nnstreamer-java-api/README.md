# Android Example with NNStreamer JAVA APIs and Camera API

## Prequisite

- To run this sample, copy nnstreamer-api library file (nnstreamer-YYYY-MM-DD.aar) into 'libs' directory.
- For image classification, user should turn on MXNet option.
- [How to build NNStreamer for Android](https://github.com/nnstreamer/nnstreamer/tree/master/api/android)
- <img src="https://user-images.githubusercontent.com/81565280/173046787-ded3aa1e-6815-41d2-b61b-b9616371a5a7.png" width="200" height="400"/>

## Introduction

1. This is a simple example to show how to use NNStreamer APIs and Android Camera API to utilize the style transfer model. (tflite model)

- The topmost view is the preview image of the camera.
- The bottom view shows style transferred image of the camera input.
- The applied style changes after several seconds.

2. This is a simple example to show how to use NNStreamer APIs and Android Camera API to utilize the image classification model. (MXNet model)

- The topmost view is the preview image of the camera.
- The bottom view shows the camera input and its classification result on the screen together.
- The applied classification results change directly as the camera screen changes.

## Screenshot

(1) ![styletransfer_nnstreamer_screenshot](./styletransfer_nnstreamer_screenshot.webp)

(2) <img src="https://user-images.githubusercontent.com/81565280/173046757-ff4c989a-bf12-4212-87a6-e583ab79506c.png" width="200" height="400"/>

