#!/usr/bin/env bash
gst-launch-1.0 \
    tensor_query_serversrc ! video/x-raw,width=640,height=480,format=RGB,framerate=0/1 ! \
        videoconvert ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter ! \
        tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! \
        tensor_filter framework=tensorflow-lite model=tflite_model/ssd_mobilenet_v2_coco.tflite ! \
        tensor_decoder mode=bounding_boxes option1=mobilenet-ssd option2=tflite_model/coco_labels_list.txt option3=tflite_model/box_priors.txt option4=640:480 option5=300:300 ! \
        videoconvert ! tensor_query_serversink
