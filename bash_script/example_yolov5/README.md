# yolov5 object detection example with simple gstreamer pipeline

## Install yolov5

```bash
$ git clone https://github.com/ultralytics/yolov5
$ cd yolov5
$ pip install -r requirements.txt
```
And download weight files in https://github.com/ultralytics/yolov5/releases/tag/v7.0

## Export to tflite and torchscript model

```bash
$ python export.py --weights=yolov5s.pt --img=320 --include tflite torchscript
$ ls
... yolov5s.torchscript yolov5s-fp16.tflite ...
```

Note that setting the input image size as 320px, rather than the default 640px to increase inference speed. You can take other weight options (n, s, m, l, x) and input image size.

## Run the object detection example

```bash
$ ./gst-launch-object-detection-yolov5-tflite.sh
$ ./gst-launch-object-detection-yolov5-torchscript.sh
```
