#!/usr/bin/env bash
gst-launch-1.0 \
    v4l2src ! videoconvert ! videoscale ! video/x-raw,format=RGB,width=640,height=480,framerate=5/1 ! mqttsink pub-topic=example/objectDetection
