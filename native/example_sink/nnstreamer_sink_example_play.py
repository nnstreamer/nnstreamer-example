#!/usr/bin/env python

"""
@file   nnstreamer_sink_example_play.py
@date   12 August 2020
@brief  Sample code for tensor sink plugin
@see    https:github.com/nnstreamer/nnstreamer-example
@author Tony Jinwoo Ahn <tony.jinwoo.ahn@gmail.com>
@bug    No known bugs.

This sample app shows video frame using two pipelines.

[1st pipeline : videotestsrc-tensor_converter-tensor_sink]
push buffer to appsrc
[2nd pipeline : appsrc-tensor_decoder-videoconvert-ximagesink]

Run example :
Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
$ python nnstreamer_sink_example_play.py

See https://lazka.github.io/pgi-docs/#Gst-1.0 for Gst API details.
"""

import os
import sys
import logging
import gi

gi.require_version('Gst', '1.0')
gi.require_version("GstApp", "1.0")

from gi.repository import Gst, GObject, GstApp

class NNStreamerExample:
    def __init__(self, argv=None):
        self.loop = None
        self.data_pipeline = None
        self.data_bus = None
        self.player_pipeline = None
        self.player_bus = None
        self.set_caps = False
        self.received = 0
        self.width = 640
        self.height = 480
        GObject.threads_init()
        Gst.init(argv)

    def run_example(self):
        """Init pipeline and run example.

        :return: None
        """
        # main loop
        self.loop = GObject.MainLoop()

        # data pipeline
        str_pipeline = 'videotestsrc is-live=TRUE ! video/x-raw,format=RGB,width=%d,height=%d ! tensor_converter ! tensor_sink name=tensor_sink' % (self.width, self.height)
        self.data_pipeline = Gst.parse_launch(str_pipeline)

        # data message callback
        data_bus = self.data_pipeline.get_bus()
        data_bus.add_signal_watch()
        data_bus.connect('message', self._data_message_cb)

        # get tensor sink element using name
        tensor_sink = self.data_pipeline.get_by_name('tensor_sink')

        # print logs
        tensor_sink.set_property('silent', False)

        # enable emit-signal
        tensor_sink.set_property('emit-signal', True)

        # tensor sink signal : new data callback
        tensor_sink.connect('new-data', self._new_data_cb)

        # player pipeline
        str_pipeline = 'appsrc name=player_src ! tensor_decoder mode=direct_video ! videoconvert ! ximagesink'
        self.player_pipeline = Gst.parse_launch(str_pipeline)

        # player message callback
        player_bus = self.player_pipeline.get_bus()
        player_bus.add_signal_watch()
        player_bus.connect('message', self._player_message_cb)

        # start pipeline
        self.data_pipeline.set_state(Gst.State.PLAYING)
        self.player_pipeline.set_state(Gst.State.PLAYING)

        # run main loop
        self.loop.run()

        # quit when received EOS or ERROR message
        self.data_pipeline.set_state(Gst.State.NULL)
        self.player_pipeline.set_state(Gst.State.NULL)

        data_bus.remove_signal_watch()
        player_bus.remove_signal_watch()

    def _data_message_cb(self, bus, message):
        """Callback for message.

        :param bus: pipeline bus
        :param message: message from pipeline
        :return: None
        """
        if message.type == Gst.MessageType.EOS:
            logging.info('[data] received eos message')
            player_src = self.player_pipeline.get_by_name('player_src')
            player_src.end_of_stream()
        elif message.type == Gst.MessageType.ERROR:
            error, debug = message.parse_error()
            logging.warning('[data][error] %s : %s', error.message, debug)
            self.loop.quit()
        elif message.type == Gst.MessageType.WARNING:
            error, debug = message.parse_warning()
            logging.warning('[data][warning] %s : %s', error.message, debug)
        elif message.type == Gst.MessageType.STREAM_START:
            logging.info('[data] received start message')

    def _player_message_cb(self, bus, message):
        """Callback for message.

        :param bus: pipeline bus
        :param message: message from pipeline
        :return: None
        """
        if message.type == Gst.MessageType.EOS:
            logging.info('[player] received eos message')
            self.loop.quit()
        elif message.type == Gst.MessageType.ERROR:
            error, debug = message.parse_error()
            logging.warning('[player][error] %s : %s', error.message, debug)
            self.loop.quit()
        elif message.type == Gst.MessageType.WARNING:
            error, debug = message.parse_warning()
            logging.warning('[player][warning] %s : %s', error.message, debug)
        elif message.type == Gst.MessageType.STREAM_START:
            logging.info('[player] received start message')

    def _new_data_cb(self, sink, buffer):
        """Callback for signal new-data.

        :param sink: tensor sink element
        :param buffer: buffer from element
        :return: None
        """
        self.received += 1
        if self.received % 150 == 0:
            logging.info('receiving new data [%d]', self.received)

        player_src = self.player_pipeline.get_by_name('player_src')
        if not self.set_caps:
            sink_pad = sink.get_static_pad('sink')
            if sink_pad is not None:
                caps = sink_pad.get_current_caps()
                if caps is not None:
                    player_src.set_caps(caps)
                    self.set_caps = True
        player_src.push_buffer(buffer.copy_deep())


if __name__ == '__main__':
    logging.getLogger().setLevel(logging.INFO)
    example = NNStreamerExample(sys.argv[1:])
    example.run_example()
