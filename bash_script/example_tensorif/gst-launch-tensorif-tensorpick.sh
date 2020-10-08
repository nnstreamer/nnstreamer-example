#!/usr/bin/env bash
gst-launch-1.0 v4l2src ! videoconvert ! video/x-raw,width=640,height=480,format=RGB,framerate=5/1 ! videocrop left=80 right=80 ! tee name=t_raw \
    t_raw. ! queue ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter ! mux_0.sink_0 \
    t_raw. ! queue ! videoconvert ! video/x-raw,format=RGBx ! burn adjustment=30 ! videoconvert ! tensor_converter ! mux_0.sink_1 \
    t_raw. ! queue  ! videoconvert ! videoscale ! video/x-raw,height=300,width=300,format=BGR ! tensor_converter ! \
        tensor_transform mode=typecast option=float32 ! tensor_transform mode=dimchg option=0:2 ! \
        tensor_filter framework=openvino model=openvino_models/face-detection-retail-0005.xml accelerator=true:cpu ! mux_0.sink_2 \
        tensor_mux name=mux_0 ! tensor_if name=tif compared_value=A_VALUE compared-value-option=2:0:0:0,2 \
                                          supplied-value=0.7 operator=GE then=TENSORPICK then-option=0 else=TENSORPICK else-option=1 \
            tif.src_0 ! queue leaky=2 max-size-buffers=2 ! tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! \
                tensor_filter framework=tensorflow-lite model=tflite_model/ssd_mobilenet_v2_coco.tflite ! \
                tensor_decoder mode=bounding_boxes option1=tflite-ssd option2=tflite_model/coco_labels_list.txt option3=tflite_model/box_priors.txt option4=480:480 option5=300:300 ! \
                compositor name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! \
                textoverlay text="Stop detecting objects if face is not detected" valignment=top halignment=left font-desc="Sans, 32" ! ximagesink sync=false async=false \
            tif.src_1 ! queue leaky=2 max-size-buffers=2 ! tensor_decoder mode=direct_video ! videoconvert ! \
            textoverlay text="Enabled when face is not detected" valignment=top halignment=left font-desc="Sans, 32" ! ximagesink sync=false async=false \
    t_raw. ! queue leaky=2 max-size-buffers=2 ! mix.
