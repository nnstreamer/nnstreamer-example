#!/usr/bin/env bash
gst-launch-1.0 \
    tensor_query_serversrc ! tensor_filter framework=tensorflow-lite model=tflite_model/ssd_mobilenet_v2_coco.tflite ! \
    tensor_decoder mode=bounding_boxes option1=mobilenet-ssd option2=tflite_model/coco_labels_list.txt option3=tflite_model/box_priors.txt option4=640:480 option5=300:300 ! \
    tensor_converter ! other/tensors,num_tensors=1,dimensions=4:640:480:1,types=uint8 ! tensor_query_serversink 
