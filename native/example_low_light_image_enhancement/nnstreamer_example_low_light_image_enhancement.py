#!/usr/bin/env python

"""
@file		nnstreamer_example_low_light_image_enhancement.py
@date		5 Jan 2022
@brief		example with TF-Lite model for low light image enhancement
@see		https://github.com/nnstreamer/nnstreamer
@author		Yelin Jeong <yelini.jeong@samsung.com>
@bug		No known bugs.

Get model by
$ cd $NNST_ROOT/bin
$ bash get-model.sh low-light-image-enhancement
"""

import os
import sys
import logging
import gi
import numpy as np

gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib

from PIL import Image

class NNStreamerExample:
    """NNStreamer example for low light image enhancement"""

    def __init__(self, argv=None):
        self.loop = None
        self.pipeline = None
        self.running = False
        self.tflite_model = ''
        self.image_name = ''

        if not self.tflite_init():
            raise Exception

        Gst.init(argv)

    def run_example(self):
        """Init pipeline and run example.
        :return: None
        """
        # main loop
        self.loop = GLib.MainLoop()

        # init pipeline
        self.pipeline = Gst.parse_launch(
            'filesrc location=' + self.image_name + ' ! pngdec ! videoscale ! '
            'videoconvert ! video/x-raw,width=600,height=400,format=RGB ! tensor_converter ! '
            'tensor_transform mode=arithmetic option=typecast:float32,add:0,div:255.0 ! '
            'tensor_filter framework=tensorflow-lite model= '+ self.tflite_model +' ! '
            'tensor_sink name=tensor_sink'
        )

        # bus and message callback
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect('message', self.on_bus_message)

        # tensor sink signal : new data callback
        tensor_sink = self.pipeline.get_by_name('tensor_sink')
        tensor_sink.connect('new-data', self.on_new_data)

        # start pipeline
        self.pipeline.set_state(Gst.State.PLAYING)
        self.running = True

        # run main loop
        self.loop.run()

        # quit when received eos or error message
        self.running = False
        self.pipeline.set_state(Gst.State.NULL)

        bus.remove_signal_watch()

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

    def on_new_data(self, sink, buffer):
        """Callback for tensor sink signal.
        :param sink: tensor sink element
        :param buffer: buffer from element
        :return: None
        """
        if self.running:
            for idx in range(buffer.n_memory()):
                mem = buffer.get_memory(idx)
                result, mapinfo = mem.map(Gst.MapFlags.READ)

                if result:
                    content_arr = np.frombuffer(mapinfo.data, dtype=np.float32)
                    content = np.reshape(content_arr,(400,600,3)) * 255.0
                    content = content.clip(0,255)
                    img = Image.fromarray(np.uint8(content))
                    img.save('low_light_enhancement_'+self.image_name)
                    img.show()
                    mem.unmap(mapinfo)
            self.loop.quit()

    def tflite_init(self):
        tflite_model = 'lite-model_zero-dce_1.tflite'
        current_folder = os.path.dirname(os.path.abspath(__file__))
        model_folder = os.path.join(current_folder,'tflite_low_light_image_enhancement')

        # check model file exists
        self.tflite_model = os.path.join(model_folder, tflite_model)
        if not os.path.exists(self.tflite_model):
            logging.error('cannot find tflite model [%s]', self.tflite_model)
            return False

        # get image name
        image_name = input("please type a png file name\n") + '.png'
        self.image_name = image_name
        if not os.path.exists(os.path.join(current_folder, image_name)):
            logging.error('cannot find image [%s]',image_name)
            return False
        return True

if __name__ == '__main__':
    example = NNStreamerExample(sys.argv[1:])
    example.run_example()
