#!/usr/bin/env bash
if [ -z "$1" ]; then
  echo "Server host address is not given. Use localhost."
  HOST="localhost"
else
  HOST="$1"
fi
if [ -z "$2" ]; then
  echo "Broker host address is not given. Use localhost."
  DEST_HOST="tcp://localhost"
else
  DEST_HOST="$1"
fi

gst-launch-1.0 \
    tensor_query_serversrc id=321 host=$HOST port=0 dest-host=$DEST_HOST dest-port=1883 topic=objectDetection connect-type=HYBRID ! \
    video/x-raw,width=640,height=480,format=RGB,framerate=0/1 ! \
    videoconvert ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter ! \
    tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! \
    tensor_filter framework=tensorflow-lite model=tflite_model/ssd_mobilenet_v2_coco.tflite ! \
    tensor_decoder mode=bounding_boxes option1=mobilenet-ssd option2=tflite_model/coco_labels_list.txt option3=tflite_model/box_priors.txt option4=640:480 option5=300:300 ! \
    videoconvert ! tensor_query_serversink id=321 connect-type=HYBRID
