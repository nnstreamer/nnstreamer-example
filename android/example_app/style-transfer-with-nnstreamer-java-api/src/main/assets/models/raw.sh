#!/bin/bash
for i in {1..28}
do
	gst-launch-1.0 -v filesrc location=style$i.jpg ! jpegdec ! videoconvert ! videoscale ! video/x-raw,format=RGB,width=256,height=256 ! filesink location=style$i.raw
done
