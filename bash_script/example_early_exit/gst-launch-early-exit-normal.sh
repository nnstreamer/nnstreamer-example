#!/usr/bin/env bash
gst-launch-1.0 \
    v4l2src ! videoscale ! videoconvert ! video/x-raw,format=RGB,width=640,height=480,framerate=5/1 ! \
    tensor_converter ! tensor_filter framework=custom model=../lib/nnstreamer/customfilters/libnnstreamer_customfilter_passthrough_variable.so ! tee name=t \
        t. ! queue leaky=2 max-size-buffers=2 ! mux.sink_0 \
        t. ! queue leaky=2 max-size-buffers=2 ! tensor_filter framework=custom model=../lib/nnstreamer/customfilters/libnnstreamer_customfilter_average.so ! mux.sink_1 \
    tensor_mux name=mux ! tensor_if name=tif compared-value=A_VALUE compared-value-option=0:0:0:0,1 \
                                    supplied-value=100 operator=GE then=TENSORPICK then-option=1 else=TENSORPICK else-option=0 \
        tif.src_0 ! queue leaky=2 max-size-buffers=2 ! join.sink_0 \
        tif.src_1 ! queue leaky=2 max-size-buffers=2 ! tensor_filter framework=custom model=../lib/nnstreamer/customfilters/libnnstreamer_customfilter_average.so ! join.sink_1 \
    join name=join ! tensor_sink
