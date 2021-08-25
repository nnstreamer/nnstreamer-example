#!/usr/bin/env bash

echo "make raw image data orange.raw"
gst-launch-1.0 filesrc location=orange.png ! pngdec ! videoscale ! videoconvert ! video/x-raw,format=RGB,width=224,height=224 ! filesink location=orange.raw

echo "make compressed data orange.gz"
gst-launch-1.0 filesrc location=orange.raw ! gzenc ! filesink location=orange.gz

echo "decompress"
gst-launch-1.0 filesrc location=orange.gz ! gzdec ! filesink location=orange.dec

echo "equality test between orange.raw and orange.dec"
cmp orange.raw orange.dec
if [[ $? == 0 ]]
then
    echo "raw file and the decompressed file are same"
else
    echo "raw file and the decompressed file are NOT same"
fi

echo "use compressed data in gstreamer pipeline"
gst-launch-1.0 filesrc location=orange.gz blocksize=-1 ! gzdec buffer-size=150528 first-buffer-size=150528 ! queue ! video/x-raw,format=RGB,height=224,width=224,framerate=0/1 ! tensor_converter ! tensor_sink

gst-launch-1.0 filesrc location=orange.gz blocksize=-1 ! gzdec buffer-size=150528 first-buffer-size=150528 ! queue ! video/x-raw,format=RGB,height=224,width=224,framerate=0/1 ! tensor_converter ! tensor_decoder mode=direct_video ! video/x-raw,format=RGB,height=224,width=224,framerate=0/1 ! videoconvert ! imagefreeze ! videoconvert ! ximagesink
