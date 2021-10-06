#!/usr/bin/env bash
gst-launch-1.0 \
    compositor name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! ximagesink \
        v4l2src ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB,framerate=10/1 ! tee name=t \
            t. ! queue ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter ! tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! queue leaky=2 max-size-buffers=2 ! \
                tensor_query_client operation=passthrough ! tensor_decoder mode=direct_video ! videoconvert ! video/x-raw,width=640,height=480,format=RGBA ! mix.sink_0 \
            t. ! queue ! mix.sink_1
