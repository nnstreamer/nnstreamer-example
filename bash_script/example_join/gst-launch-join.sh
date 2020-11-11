#!/usr/bin/env bash
gst-launch-1.0 v4l2src ! videoconvert ! videoscale ! video/x-raw, format=RGB, width=640, height=480, framerate=30/1 ! tensor_converter ! mux.sink_0 \
               videotestsrc ! videoconvert ! videoscale ! video/x-raw, format=RGB, width=640, height=480, framerate=30/1 ! tensor_converter ! mux.sink_1 \
                              tensor_mux name=mux ! tensor_if name=tif compared_value=TENSOR_AVERAGE_VALUE compared-value-option=0 \
                              supplied-value=100 operator=GT then=TENSORPICK then-option=0 else=TENSORPICK else-option=1 \
               tif.src_0 ! queue ! join.sink_0 \
               tif.src_1 ! queue ! join.sink_1 \
               join name=join ! tensor_decoder mode=direct_video ! videoconvert ! ximagesink
