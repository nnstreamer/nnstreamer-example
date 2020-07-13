#!/usr/bin/env bash
# The tcp server converts the tensor to flatbuf and transmits it.
# The sample app was tested in Ubuntu and rpi4(Tizen IoT platform).

if [ -n "$1" ] && [ -n "$2" ]; then
  ip=$1
  port=$2
else
  echo -e "Please allocates the host/IP and port number"
  echo -e "e.x) ./gst-launch-image-segmentation-edgetpu-tcpserver.sh 192.168.0.1 5001"
  exit 1
fi

gst-launch-1.0 v4l2src ! videoconvert ! videoscale ! video/x-raw, width=257, height=257, framerate=2/1, format=RGB ! tensor_converter ! \
tensor_decoder mode=flatbuf ! gdppay ! tcpserversink host=${ip} port=${port}
