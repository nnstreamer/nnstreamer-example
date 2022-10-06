#!/usr/bin/env bash
if [ -z "$1" ]; then
  echo "Client host address is not given. Use localhost."
  HOST="localhost"
else
  HOST="$1"
fi
if [ -z "$2" ]; then
  echo "Client host address is not given. Use localhost."
  DEST_HOST="tcp://localhost"
else
  DEST_HOST="$2"
fi

gst-launch-1.0 \
    compositor name=mix sink_0::zorder=2 sink_1::zorder=1 ! videoconvert ! ximagesink \
        v4l2src ! videoconvert ! videoscale ! video/x-raw,width=640,height=480,format=RGB,framerate=10/1 ! tee name=t \
            t. ! queue ! tensor_query_client connect-type=HYBRID host=$HOST port=0 dest-host=$DEST_HOST dest-port=1883 topic=objectDetection ! videoconvert ! mix.sink_0 \
            t. ! queue ! mix.sink_1
