#!/usr/bin/env bash
# The tcp client converts flatbuf into video, run image segmentation using the edgeTPU and display it.
# The sample app was tested in Ubuntu and rpi4(Tizen IoT platform).

if [ -n "$1" ] && [ -n "$2" ]; then
  ip=$1
  port=$2
else
  echo -e "Please allocates the host/IP and port number"
  echo -e "e.x) ./gst-launch-image-segmentation-edgetpu-tcpserver.sh 192.168.0.1 5001"
  exit 1
fi

FILE=./tflite_img_segment_model/deeplabv3_257_mv_gpu.tflite
if [ ! -f "$FILE" ]; then
  echo -e "Cannot find deeplabv3_257_mv_gpu.tflite model files"
  echo -e "Please enter as below to download and locate the model."
  echo -e "$ cd $NNST_ROOT/bin/"
  echo -e "$ ./get-model.sh image-segmentation-tflite"
  exit 1
fi

gst-launch-1.0 -v \
    videomixer name=mix sink_0::alpha=0.7 sink_1::alpha=0.6 ! videoconvert ! autovideosink \
    tcpclientsrc host=${ip} port=${port} ! gdpdepay ! other/flatbuf-tensor ! tensor_converter ! tee name=t \
    t. ! queue ! tensor_decoder mode=direct_video ! videoconvert ! videoscale ! video/x-raw,format=RGB,width=257,height=257,framerate=2/1 ! mix. \
    t. ! queue ! tensor_transform mode=arithmetic option=typecast:float32,div:255.0 ! \
        tensor_filter framework=edgetpu model=${FILE} custom=device_type:usb ! \
        tensor_decoder mode=image_segment option1=tflite-deeplab ! mix.
