#!/usr/bin/env bash
gst-launch-1.0 \
    compositor name=mix sink_0::zorder=1 sink_1::zorder=2 ! videoconvert ! autovideosink sync=false \
    v4l2src ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB,framerate=10/1 ! tee name=t \
      t. ! queue ! mix.sink_0 \
      t. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! videoscale ! video/x-raw,height=320,width=544,format=BGR ! \
        tensor_converter ! tensor_transform mode=typecast option=float32 ! tensor_transform mode=dimchg option=0:2 ! \
        tensor_filter framework=openvino model=openvino_models/person-detection-retail-0013.xml accelerator=true:npu ! \
        tensor_decoder mode=bounding_boxes option1=ov-person-detection option4=640:480 option5=300:300 ! mix.sink_1
