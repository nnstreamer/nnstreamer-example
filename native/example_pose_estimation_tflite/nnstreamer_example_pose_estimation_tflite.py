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
import math
import numpy as np
import ctypes
import cairo

gi.require_version('Gst', '1.0')
gi.require_foreign('cairo')
from gi.repository import Gst, GObject

DEBUG = False

class NNStreamerExample:
    """NNStreamer example for face detection."""

    def __init__(self, argv=None):
        self.loop = None
        self.pipeline = None
        self.running = False
        self.video_caps = None

        self.VIDEO_WIDTH = 640
        self.VIDEO_HEIGHT = 480
        self.MODEL_INPUT_HEIGHT = 257
        self.MODEL_INPUT_WIDTH = 257

        self.KEYPOINT_SIZE = 17
        self.OUTPUT_STRIDE = 32
        self.GRID_XSIZE = 9
        self.GRID_YSIZE = 9

        self.SCORE_THRESHOLD = 0.7

        self.tflite_model = ''
        self.tflite_labels = []
        self.kps = []

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
            'tensor_sink name=tensor_sink'
        )

        # bus and message callback
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect('message', self.on_bus_message)

        # tensor sink signal : new data callback
        tensor_sink = self.pipeline.get_by_name('tensor_sink')
        tensor_sink.connect('new-data', self.new_data_cb)

        tensor_res = self.pipeline.get_by_name('tensor_res')
        tensor_res.connect('draw', self.draw_overlay_cb)
        tensor_res.connect('caps-changed', self.prepare_overlay_cb)

        # start pipeline
        self.pipeline.set_state(Gst.State.PLAYING)
        self.running = True

        self.set_window_title('img_tensor', 'An NNStreamer Example - Single Person Pose Estimation')

        # run main loop
        self.loop.run()

        # quit when received eos or error message
        self.running = False
        self.pipeline.set_state(Gst.State.NULL)

        bus.remove_signal_watch()

    def tflite_init(self):
        """
        :return: True if successfully initialized
        """
        tflite_model = 'posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite'
        tflite_label = 'key_point_labels.txt'

        current_folder = os.path.dirname(os.path.abspath(__file__))
        model_folder = os.path.join(current_folder, 'tflite_pose_estimation')

        self.tflite_model = os.path.join(model_folder, tflite_model)
        if not os.path.exists(self.tflite_model):
            logging.error('cannot find tflite model [%s]', self.tflite_model)
            return False

        label_path = os.path.join(model_folder, tflite_label)
        try:
            with open(label_path, 'r') as label_file:
                for line in label_file.readlines():
                    self.tflite_labels.append(line)
        except FileNotFoundError:
            logging.error('cannot find tflite label [%s]', label_path)
            return False

        logging.info('finished to load labels, total [%d]', len(self.tflite_labels))
        return True

    # @brief Callback for tensor sink signal.
    def new_data_cb(self, sink, buffer):
        if self.running:
            if buffer.n_memory() != 4:
                return False
            #  tensor type is float32.
            #  [0] dim of heatmap := KEY_POINT_NUMBER : height_after_stride : 
            #       width_after_stride: 1 (17:9:9:1)
            #  [1] dim of offsets := 34 (concat of y-axis and x-axis of offset vector) :
            #       hegiht_after_stride : width_after_stride : 1 (34:9:9:1)
            #  [2] dim of displacement forward (not used for single person pose estimation)
            #  [3] dim of displacement backward (not used for single person pose estimation)

            # heatmap
            mem_heatmap = buffer.peek_memory(0)
            result1, info_heatmap = mem_heatmap.map(Gst.MapFlags.READ)
            if result1:
                assert info_heatmap.size == 17 * 9 * 9 * 1 * 4
                decoded_heatmap = list(np.frombuffer(info_heatmap.data, dtype=np.float32))  # decode bytestrings to float list
            
            # offset
            mem_offset = buffer.peek_memory(1)
            result2, info_offset = mem_offset.map(Gst.MapFlags.READ)
            if result2:
                assert info_offset.size == 34 * 9 * 9 * 1 * 4
                decoded_offset = list(np.frombuffer(info_offset.data, dtype=np.float32)) # decode bytestrings to float list
            
            self.kps.clear()
            for keyPointIdx in range(0, self.KEYPOINT_SIZE):
                yPosIdx = -1
                xPosIdx = -1
                highestScore = -float("1.0842021724855044e-19")
                currentScore = 0

                # find the index of key point with highestScore in 9 X 9 grid
                for yIdx in range(0, self.GRID_YSIZE):
                    for xIdx in range(0, self.GRID_XSIZE):
                        current = decoded_heatmap[(yIdx * self.GRID_YSIZE + xIdx) * self.KEYPOINT_SIZE + keyPointIdx]
                        currentScore = 1.0 / (1.0 + math.exp(-current))
                        if(currentScore > highestScore):
                            yPosIdx = yIdx
                            xPosIdx = xIdx
                            highestScore = currentScore

                yOffset = decoded_offset[(yPosIdx * self.GRID_YSIZE + xPosIdx) * self.KEYPOINT_SIZE * 2 + keyPointIdx]
                xOffset = decoded_offset[(yPosIdx * self.GRID_YSIZE + xPosIdx) * self.KEYPOINT_SIZE * 2 + self.KEYPOINT_SIZE + keyPointIdx]

                yPosition = (yPosIdx / (self.GRID_YSIZE - 1)) * self.MODEL_INPUT_HEIGHT + yOffset
                xPosition = (xPosIdx / (self.GRID_XSIZE - 1)) * self.MODEL_INPUT_WIDTH + xOffset

                obj = {
                    'y': yPosition,
                    'x': xPosition,
                    'label': keyPointIdx,
                    'score': highestScore
                }

                self.kps.append(obj)

            mem_heatmap.unmap(info_heatmap)
            mem_offset.unmap(info_offset)

    # @brief Store the information from the caps that we are interested in.
    def prepare_overlay_cb(self, overlay, caps):
        self.video_caps = caps

    # @brief Callback to draw the overlay.
    def draw_overlay_cb(self, overlay, context, timestamp, duration):
        if self.video_caps == None or not self.running:
            return

        # mutex_lock alternative required
        kpts = self.kps
        # mutex_unlock alternative needed
        
        context.select_font_face('Sans', cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        context.set_font_size(8.0)

        for obj in kpts:
            score = obj['score']
            if score < self.SCORE_THRESHOLD:
                continue

            label = self.tflite_labels[obj['label']][:-1]
            y = obj['y'] * self.VIDEO_HEIGHT / self.MODEL_INPUT_HEIGHT
            x = obj['x'] * self.VIDEO_WIDTH / self.MODEL_INPUT_WIDTH

            # draw key point
            context.set_source_rgb(0.85, 0.2, 0.2)
            context.arc(x, y, 3, 0, 2 * math.pi)
            context.stroke_preserve()
            context.fill()

            # draw the text of each key point
            context.move_to(x + 5, y + 17)
            context.text_path(label)
            context.set_source_rgb(0.7, 0.3, 0.5)
            context.fill_preserve()
            context.set_source_rgb(0.7, 0.3, 0.5)
            context.set_line_width(0.2)
            context.stroke()
            context.fill_preserve()

        # draw body lines
        context.set_source_rgb(0.85, 0.2, 0.2)
        context.set_line_width(3)

        self.draw_line(overlay, context, kpts, 5, 6)
        self.draw_line(overlay, context, kpts, 5, 7)
        self.draw_line(overlay, context, kpts, 7, 9)
        self.draw_line(overlay, context, kpts, 6, 8)
        self.draw_line(overlay, context, kpts, 8, 10)
        self.draw_line(overlay, context, kpts, 6, 12)
        self.draw_line(overlay, context, kpts, 5, 11)
        self.draw_line(overlay, context, kpts, 11, 13)
        self.draw_line(overlay, context, kpts, 13, 15)
        self.draw_line(overlay, context, kpts, 12, 11)
        self.draw_line(overlay, context, kpts, 12, 14)
        self.draw_line(overlay, context, kpts, 14, 16)

        context.stroke()

    def draw_line(self, overlay, context, kpts, from_key, to_key):
        if len(kpts) < 1:
            return
        if kpts[from_key]['score'] < self.SCORE_THRESHOLD or kpts[to_key]['score'] < self.SCORE_THRESHOLD:
            return
        
        context.move_to(kpts[from_key]['x'] * self.VIDEO_WIDTH / self.MODEL_INPUT_WIDTH, kpts[from_key]['y'] * self.VIDEO_HEIGHT / self.MODEL_INPUT_HEIGHT)
        context.line_to(kpts[to_key]['x'] * self.VIDEO_WIDTH / self.MODEL_INPUT_WIDTH, kpts[to_key]['y'] * self.VIDEO_HEIGHT / self.MODEL_INPUT_HEIGHT)

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


