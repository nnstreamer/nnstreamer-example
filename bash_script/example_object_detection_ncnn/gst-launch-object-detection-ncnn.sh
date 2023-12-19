#!/usr/bin/env bash
gst-launch-1.0 \
    v4l2src ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB ! tee name=t \
    t. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! videoscale ! video/x-raw,width=300,height=300,format=BGR ! tensor_converter ! \
    tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tensor_transform mode=transpose option=1:2:0:3 ! \
    tensor_filter framework=ncnn model=ncnn_model/mobilenetv2_ssdlite_voc.param,ncnn_model/mobilenetv2_ssdlite_voc.bin input=300:300:3 inputtype=float32 output=26:5322:1 outputtype=float32 custom=use_yolo_decoder:true ! \
    tensor_decoder mode=bounding_boxes option1=yolov5 option2=ncnn_model/voc_labels.txt option3=0 option4=640:480 option5=300:300 ! \
    compositor name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! autovideosink \
    t. ! queue leaky=2 max-size-buffers=10 ! mix.
