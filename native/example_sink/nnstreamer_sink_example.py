#!/usr/bin/env python

"""
@file   nnstreamer_sink_example.py
@date   3 August 2020
@brief  Sample code for tensor sink plugin
@see    https:github.com/nnstreamer/nnstreamer-example
@author Tony Jinwoo Ahn <tony.jinwoo.ahn@gmail.com>
@bug    No known bugs.

Simple example to init tensor sink element and get data.

Run example :
Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plugin.
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
$ python nnstreamer_sink_example.py video
$ python nnstreamer_sink_example.py audio
$ python nnstreamer_sink_example.py text

See https://lazka.github.io/pgi-docs/#Gst-1.0 for Gst API details.
"""

import os
import sys
import logging
import gi

gi.require_version('Gst', '1.0')
gi.require_version("GstApp", "1.0")

from gi.repository import Gst, GObject, GstApp

class TestMediaType:
    TEST_TYPE_VIDEO = 0
    TEST_TYPE_AUDIO = 1
    TEST_TYPE_TEXT = 2

class NNStreamerExample:
    def __init__(self, argv=None):
        self.media_type = None
        self.loop = None
        self.pipeline = None
        self.received = 0;

        GObject.threads_init()
        Gst.init(argv)

        self.media_type = self.parse_arg(argv)
        if self.media_type == TestMediaType.TEST_TYPE_VIDEO:
            print("video 100 buffers")
        elif self.media_type == TestMediaType.TEST_TYPE_AUDIO:
            print("audio 30 buffers")
        elif self.media_type == TestMediaType.TEST_TYPE_TEXT:
            print("text 20 buffers")
        else:
            self.usage()
            sys.exit()

    def run_example(self):
        """Init pipeline and run example.

        :return: None
        """
        # main loop
        self.loop = GObject.MainLoop()

        # init pipeline
        if self.media_type == TestMediaType.TEST_TYPE_VIDEO:
            # video 640x480 30fps 100 buffers
            self.pipeline = Gst.parse_launch(
                'videotestsrc num-buffers=100 ! video/x-raw,format=RGB,width=640,height=480 ! '
                'tensor_converter ! tensor_sink name=tensor_sink'
            )
        elif self.media_type == TestMediaType.TEST_TYPE_AUDIO:
            # audio sample rate 16000 (16 bits, signed, little endian) 30 buffers
            self.pipeline = Gst.parse_launch(
                'audiotestsrc num-buffers=30 ! audio/x-raw,format=S16LE,rate=16000 ! '
                'tensor_converter ! tensor_sink name=tensor_sink'
            )
        elif self.media_type == TestMediaType.TEST_TYPE_TEXT:
            # text 20 buffers
            self.pipeline = Gst.parse_launch(
                'appsrc name=appsrc ! text/x-raw,format=utf8 ! '
                'tensor_converter input-dim=24 ! tensor_sink name=tensor_sink'
            )
        else:
            print("Wrong media type")
            sys.exit()

        # bus and message callback
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect('message', self.on_bus_message)

        # get tensor sink element using name
        tensor_sink = self.pipeline.get_by_name('tensor_sink')

        # print logs
        tensor_sink.set_property('silent', False)

        # enable emit-signal
        tensor_sink.set_property('emit-signal', True)

        # tensor sink signal : new data callback
        tensor_sink.connect('new-data', self.on_new_data)

        # tensor sink signal : stream-start callback, optional
        tensor_sink.connect('stream-start', self.on_stream_start)

        # tensor sink signal : eos callback, optional
        tensor_sink.connect('eos', self.on_eos)

        GObject.timeout_add(20, self.on_timer)

        # start pipeline
        self.pipeline.set_state(Gst.State.PLAYING)

        # run main loop
        self.loop.run()

        # quit when received EOS or ERROR message
        self.pipeline.set_state(Gst.State.NULL)

        bus.remove_signal_watch()
        logging.info('total received %d', self.received)

    def parse_arg(self, argv):
        """Parse argument to determine media type.

        :param argv: argument containing media type
        :return: media_type
        """
        media_type = ''.join(argv)
        if media_type == "video":
            return TestMediaType.TEST_TYPE_VIDEO
        elif media_type == "audio":
            return TestMediaType.TEST_TYPE_AUDIO
        elif media_type == "text":
            return TestMediaType.TEST_TYPE_TEXT
        else:
            # parse error
            self.usage()
            sys.exit()

    def usage(self):
        """Usage guide.

        :return: None
        """
        print("Usage: ")
        print("$ python nnstreamer_sink_example.py video")
        print("$ python nnstreamer_sink_example.py audio")
        print("$ python nnstreamer_sink_example.py text")

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

    def parse_caps(self, caps):
        """Function to print caps.

        :param caps: GstCaps
        :return: None
        """
        caps_size = caps.get_size()
        for i in range(caps_size):
            structure = caps.get_structure(i)
            str = structure.to_string()
            logging.info('[%d] %s', i, str)

    def on_new_data(self, sink, buffer):
        """Callback for signal new-data.

        :param sink: tensor sink element
        :param buffer: buffer from element
        :return: None
        """
        self.received += 1
        if self.received % 150 == 0:
            logging.info('receiving new data [%d]', self.received)

        # example to get data
        for idx in range(buffer.n_memory()):
            mem = buffer.peek_memory(idx)
            result, mapinfo = mem.map(Gst.MapFlags.READ)
            if result:
                if self.media_type == TestMediaType.TEST_TYPE_TEXT:
                    logging.info('received %d [%s]', mapinfo.size, mapinfo.data)
                else:
                    logging.info('received %d', mapinfo.size)
                mem.unmap(mapinfo)

        # example to get caps */
        sink_pad = sink.get_static_pad('sink')
        if sink_pad is not None:
            # negotiated
            caps = sink_pad.get_current_caps()
            if caps is not None:
                self.parse_caps(caps)
            # template
            caps = sink_pad.get_pad_template_caps()
            if caps is not None:
                self.parse_caps(caps)

    def on_stream_start(self, sink):
        """Callback for signal stream-start.

        :param sink: tensor sink element
        :return: None
        """
        logging.info('stream start callback')

    def on_eos(self, sink):
        """Callback for signal eos.

        :param sink: tensor sink element
        :return: None
        """
        logging.info('eos callback')

    def on_timer(self):
        """Timer callback to push buffer.

        :return: True to ensure the timer continues
        """
        buffer_index = self.received + 1
        appsrc = self.pipeline.get_by_name('appsrc')

        if self.media_type == TestMediaType.TEST_TYPE_TEXT:
            # send 20 text buffers
            if buffer_index > 20:
                if appsrc.end_of_stream() != Gst.FlowReturn.OK:
                    logging.info('failed to indicate eos')
                return False

            text_data = "example for text [{:02d}/20]".format(buffer_index)
            buf = Gst.Buffer.new_wrapped(text_data)

            buf.pts = 20 * Gst.MSECOND * buffer_index
            buf.duration = 20 * Gst.MSECOND

            if appsrc.push_buffer(buf) != Gst.FlowReturn.OK:
                logging.info('failed to push buffer [%d]', buffer_index)
        else:
            # nothing to do
            return False
        return True


if __name__ == '__main__':
    logging.getLogger().setLevel(logging.INFO)
    example = NNStreamerExample(sys.argv[1:])
    example.run_example()
