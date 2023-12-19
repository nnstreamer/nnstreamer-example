#!/usr/bin/env bash
gst-launch-1.0 \
    textoverlay name=overlay font-desc=Sans,24 ! videoconvert ! ximagesink name=img_test v4l2src name=cam_src ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB ! tee name=t_raw \
    t_raw. ! queue ! overlay.video_sink \
    t_raw. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! videoscale ! video/x-raw,width=227,height=227,format=BGR ! tensor_converter ! \
    tensor_transform mode=arithmetic option=typecast:float32,add:-127.5 ! tensor_transform mode=transpose option=1:2:0:3 ! \
    tensor_filter framework=ncnn model=ncnn_model/squeezenet_v1.1.param,ncnn_model/squeezenet_v1.1.bin input=227:227:3 inputtype=float32 output=1000:1 outputtype=float32 ! \
    tensor_decoder mode=image_labeling option1=ncnn_model/squeezenet_labels.txt ! \
    overlay.text_sink
