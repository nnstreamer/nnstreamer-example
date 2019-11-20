#!/usr/bin/env bash
gst-launch-1.0 \
    v4l2src name=cam_src ! videoscale ! videoconvert ! video/x-raw,width=640,height=480,format=RGB,framerate=30/1 ! tee name=t \
    t. ! queue leaky=2 max-size-buffers=2 ! videoscale ! tensor_converter ! \
        tensor_filter framework=tensorflow model=tf_model/ssdlite_mobilenet_v2.pb \
            input=3:640:480:1 inputname=image_tensor inputtype=uint8 \
            output=1,100:1,100:1,4:100:1 \
            outputname=num_detections,detection_classes,detection_scores,detection_boxes \
            outputtype=float32,float32,float32,float32 ! \
        tensor_decoder mode=bounding_boxes option1=tf-ssd option2=tf_model/coco_labels_list.txt option4=640:480 option5=640:480 ! \
        compositor name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! ximagesink \
    t. ! queue leaky=2 max-size-buffers=10 ! mix.
