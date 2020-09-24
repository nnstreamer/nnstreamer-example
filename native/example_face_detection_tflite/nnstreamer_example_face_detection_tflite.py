#!/usr/bin/env python

"""
@file		nnstreamer_example_face_detection_tflite.py
@date		4 Oct 2020
@brief		Tensor stream example with filter
@see		https://github.com/nnsuite/nnstreamer
@author		SSAFY Team 1 <jangjongha.sw@gmail.com>
@bug		No known bugs.
NNStreamer example for face detection using tensorflow-lite.
Pipeline :
v4l2src -- (TBU)
This app displays video sink.
'tensor_filter' for image classification.
Get model by
$ cd $NNST_ROOT/bin
$ bash get-model.sh face-detection-tflite
'tensor_sink' updates classification result to display in textoverlay.
Run example :
Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plugin.
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
$ python nnstreamer_example_face_detection_tflite.py
See https://lazka.github.io/pgi-docs/#Gst-1.0 for Gst API details.
"""

import os
import sys
import gi
import logging

gi.require_version('Gst', '1.0')
from gi.repository import Gst, GObject

class NNStreamerExample:
    """NNStreamer example for face detection."""
    
    def __init__(self, argv=None):
        self.loop = None
        self.pipeline = None
        self.running = False
        self.tflite_model = ''
        self.tflite_labels = []

        GObject.threads_init()
        Gst.init(argv)

    def on_bus_message(self, bus, message):
        """Callback for message.
        :param bus: pipeline bus
        :param message: message from pipeline
        :return: None
        """
        if message.type == Gst.MessageType.EOS:
            logging.info('received eos message')
            self.loop.quit()
        elif message.type == Gst.MessageType.ERROR:
            error, debug = message.parse_error()
            logging.warning('[error] %s : %s', error.message, debug)
            self.loop.quit()
        elif message.type == Gst.MessageType.WARNING:
            error, debug = message.parse_warning()
            logging.warning('[warning] %s : %s', error.message, debug)
        elif message.type == Gst.MessageType.STREAM_START:
            logging.info('received start message')
        elif message.type == Gst.MessageType.QOS:
            data_format, processed, dropped = message.parse_qos_stats()
            format_str = Gst.Format.get_name(data_format)
            logging.debug('[qos] format[%s] processed[%d] dropped[%d]', format_str, processed, dropped)

    def set_window_title(self, name, title):
        """Set window title.
        :param name: GstXImageasink element name
        :param title: window title
        :return: None
        """
        element = self.pipeline.get_by_name(name)
        if element is not None:
            pad = element.get_static_pad('sink')
            if pad is not None:
                tags = Gst.TagList.new_empty()
                tags.add_value(Gst.TagMergeMode.APPEND, 'title', title)
                pad.send_event(Gst.Event.new_tag(tags))

    def run_example(self):
        """Init pipeline and run example.
        :return: None
        """

        print("Run: NNStreamer example for face detection.")

        # main loop
        self.loop = GObject.MainLoop()

        # init pipeline
        # Currently Only only runs video from webcam. More features TBU.
        self.pipeline = Gst.parse_launch(
            'v4l2src name=cam_src ! videoconvert ! videoscale ! '
            'video/x-raw,width=640,height=480,format=RGB ! videoconvert ! xvimagesink name=img_tensor'
        )

        # bus and message callback
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect('message', self.on_bus_message)

        # start pipeline
        self.pipeline.set_state(Gst.State.PLAYING)
        self.running = True

        self.set_window_title('img_tensor', 'NNStreamer Face Detection Example')

        # run main loop
        self.loop.run()

        # quit when received eos or error message
        self.running = False
        self.pipeline.set_state(Gst.State.NULL)

        bus.remove_signal_watch()

if __name__ == '__main__':
    example = NNStreamerExample(sys.argv[1:])
    example.run_example()
    
