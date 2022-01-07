#!/usr/bin/env python

"""
@file		nnstreamer_example_pose_estimation.py
@date		4 Oct 2020
@brief		Working in Progress
@see		https://github.com/nnstreamer/nnstreamer
@author		Soonbeen Kim <ksb940925@gmail.com>
@author		Jongha Jang <jangjongha.sw@gmail.com>
@bug		No known bugs.

This code is a Python port of Tensor stream example with TF-Lite model for pose estimation.

Get model by
$ cd $NNST_ROOT/bin
$ bash get-model.sh pose-estimation-tflite

nnstreamer_example_pose_estimation_tflite.py

Run example :
Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plugin.
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
$ python3 nnstreamer_example_pose_estimation_tflite.py

See https://lazka.github.io/pgi-docs/#Gst-1.0 for Gst API details.

Required model and resources are stored at below link
https://storage.googleapis.com/download.tensorflow.org/models/tflite/posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite
and make text file of key point labels for this model (total 17 key points include nose, left ear, right ankle, etc.)
"""

import os
import sys
import gi
import logging

gi.require_version('Gst', '1.0')
from gi.repository import Gst, GObject

DEBUG = False

class NNStreamerExample:
    """NNStreamer example for face detection."""

    def __init__(self, argv=None):
        self.loop = None
        self.pipeline = None

        self.VIDEO_WIDTH = 640
        self.VIDEO_HEIGHT = 480
        self.MODEL_INPUT_HEIGHT = 257
        self.MODEL_INPUT_WIDTH = 257


        self.tflite_model = ''
        self.label_path = ''

        if not self.tflite_init():
            raise Exception

        GObject.threads_init()
        Gst.init(argv)

    def run_example(self):
        """Init pipeline and run example.
        :return: None
        """

        print("Run: NNStreamer example for pose estimation.")

        # main loop
        self.loop = GObject.MainLoop()

        # init pipeline
        self.pipeline = Gst.parse_launch(
            'v4l2src name=src ! videoconvert ! videoscale ! '
            'video/x-raw,width=' + str(self.VIDEO_WIDTH) + ',height=' + str(self.VIDEO_HEIGHT) + ',format=RGB ! tee name=t_raw '
            't_raw. ! queue ! videoconvert ! cairooverlay name=tensor_res ! '
            'ximagesink name=img_tensor '
            't_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! '
            'video/x-raw,width=' + str(self.MODEL_INPUT_WIDTH) + ',height=' + str(self.MODEL_INPUT_HEIGHT) + ',format=RGB ! tensor_converter ! '
            'tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! '
            'tensor_filter framework=tensorflow-lite model=' + self.tflite_model + ' ! '
            'tensor_decoder mode=pose_estimation option1=640:480 option2=257:257 option3=' +self.label_path + ' option4=heatmap-offset ! '
            'compositor name=mix sink_0::zorder=1 sink_1::zorder=0 ! videoconvert ! ximagesink '
            't_raw. ! queue ! mix.'
        )

        # bus and message callback
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect('message', self.on_bus_message)

        # start pipeline
        self.pipeline.set_state(Gst.State.PLAYING)

        # run main loop
        self.loop.run()

        # quit when received eos or error message
        self.pipeline.set_state(Gst.State.NULL)

        bus.remove_signal_watch()

    def tflite_init(self):
        """
        :return: True if successfully initialized
        """
        tflite_model = 'posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite'
        tflite_label = 'point_labels.txt'

        current_folder = os.path.dirname(os.path.abspath(__file__))
        model_folder = os.path.join(current_folder, 'tflite_pose_estimation')

        self.tflite_model = os.path.join(model_folder, tflite_model)
        if not os.path.exists(self.tflite_model):
            logging.error('cannot find tflite model [%s]', self.tflite_model)
            return False

        self.label_path = os.path.join(model_folder, tflite_label)
        if not os.path.exists(self.label_path):
            logging.error('cannot find point label [%s]', self.label_path)
            return False

        return True


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

if __name__ == '__main__':
    example = NNStreamerExample(sys.argv[1:])
    example.run_example()
