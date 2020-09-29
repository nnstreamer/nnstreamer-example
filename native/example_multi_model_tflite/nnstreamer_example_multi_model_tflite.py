#!/usr/bin/env python

"""
@file		nnstreamer_example_multi_model_tflite.py
@date		4 Oct 2020
@brief		Tensor stream example with multi TF-Lite model for object and face detection.
@see		https://github.com/nnstreamer/nnstreamer-example
@author		SSAFY Team 1 <jangjongha.sw@gmail.com>
@bug		No known bugs.

This code is a example of implementing multimodel-ssd using object detection model and face detection model.

Pipeline :
v4l2src -- tee -- videoconvert -- cairooverlay -- ximagesink
            |
            --- videoscale -- tensor_converter -- tensor_transform -- tee -- tensor_filter -- tensor_sink
                                                                       |
                                                                       -- tensor_filter -- tensor_sink

Get model by
$ cd $NNST_ROOT/bin
$ bash get-model.sh object-detection-tflite
$ bash get-model.sh face-detection-tflite

Run example :
Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plugin.
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
$ python3 nnstreamer_example_multi_model_tflite.py

See https://lazka.github.io/pgi-docs/#Gst-1.0 for Gst API details.

Required model and resources are stored at below link
https://github.com/nnsuite/testcases/tree/master/DeepLearningModels/tensorflow-lite/ssd_mobilenet_v2_coco
"""

import os
import sys
import gi
import logging
import math
import numpy as np
import cairo

gi.require_version('Gst', '1.0')
gi.require_foreign('cairo')
from gi.repository import Gst, GObject

DEBUG = False

class NNStreamerExample:
    """NNStreamer example for Object and Face Detection."""

    def __init__(self, argv=None):
        self.loop = None
        self.pipeline = None
        self.running = False
        self.video_caps = None

        self.BOX_SIZE = 4
        self.FACE_LABEL_SIZE = 2
        self.OBJECT_LABEL_SIZE = 91
        self.DETECTION_MAX = 1917
        self.MAX_OBJECT_DETECTION = 5

        self.Y_SCALE = 10.0
        self.X_SCALE = 10.0
        self.H_SCALE = 5.0
        self.W_SCALE = 5.0

        self.VIDEO_WIDTH = 640
        self.VIDEO_HEIGHT = 480
        self.MODEL_WIDTH = 300
        self.MODEL_HEIGHT = 300

        self.tflite_face_model = ''
        self.tflite_object_model = ''
        self.tflite_box_priors = []
        self.tflite_face_labels = []
        self.tflite_object_labels = []
        self.detected_faces = []
        self.detected_objects = []

        if not self.tflite_init():
            raise Exception

        GObject.threads_init()
        Gst.init(argv)

    def tflite_init(self):
        """
        :return: True if successfully initialized
        """
        tflite_face_model = 'detect_face.tflite'
        tflite_face_label = 'labels_face.txt'
        tflite_object_model = 'ssd_mobilenet_v2_coco.tflite'
        tflite_object_label = 'coco_labels_list.txt'
        tflite_box_prior = "box_priors.txt"

        current_folder = os.path.dirname(os.path.abspath(__file__))
        model_folder = os.path.join(current_folder, 'tflite_model')

        self.tflite_face_model = os.path.join(model_folder, tflite_face_model)
        if not os.path.exists(self.tflite_face_model):
            logging.error('cannot find tflite model [%s]', self.tflite_face_model)
            return False

        self.tflite_object_model = os.path.join(model_folder, tflite_object_model)
        if not os.path.exists(self.tflite_object_model):
            logging.error('cannot find tflite model [%s]', self.tflite_object_model)
            return False

        label_path = os.path.join(model_folder, tflite_face_label)
        try:
            with open(label_path, 'r') as label_file:
                for line in label_file.readlines():
                    self.tflite_face_labels.append(line)
        except FileNotFoundError:
            logging.error('cannot find tflite label [%s]', label_path)
            return False

        label_path = os.path.join(model_folder, tflite_object_label)
        try:
            with open(label_path, 'r') as label_file:
                for line in label_file.readlines():
                    self.tflite_object_labels.append(line)
        except FileNotFoundError:
            logging.error('cannot find tflite label [%s]', label_path)
            return False

        box_prior_path = os.path.join(model_folder, tflite_box_prior)
        try:
            with open(box_prior_path, 'r') as box_prior_file:
                for line in box_prior_file.readlines():
                    datas = list(map(float, line.split()))
                    self.tflite_box_priors.append(datas)
        except FileNotFoundError:
            logging.error('cannot find tflite label [%s]', box_prior_path)
            return False

        print('finished to load face labels, total [{}]'.format(len(self.tflite_face_labels)))
        print('finished to load object labels, total [{}]'.format(len(self.tflite_object_labels)))
        print('finished to load box_priors, total [{}]'.format(len(self.tflite_box_priors)))
        return True

    def run_example(self):
        """Init pipeline and run example.
        :return: None
        """

        print("Run: NNStreamer example for multimodel implementation.")

        # main loop
        self.loop = GObject.MainLoop()

        # init pipeline
        self.pipeline = Gst.parse_launch(
            'v4l2src name=cam_src ! videoconvert ! videoscale ! '
            'video/x-raw,width=' + str(self.VIDEO_WIDTH) + ',height=' + str(self.VIDEO_HEIGHT) + ',format=RGB ! tee name=t_raw '
            't_raw. ! queue ! videoconvert ! cairooverlay name=tensor_res ! ximagesink name=img_tensor '
            't_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! video/x-raw,width=' + str(self.MODEL_WIDTH) + ',height=' + str(self.MODEL_HEIGHT) + ' ! tensor_converter ! '
            'tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tee name=t_ssd '
            't_ssd. ! queue leaky=2 max-size-buffers=2 ! tensor_filter framework=tensorflow-lite model=' + self.tflite_face_model + ' ! tensor_sink name=res_face '
            't_ssd. ! queue leaky=2 max-size-buffers=2 ! tensor_filter framework=tensorflow-lite model=' + self.tflite_object_model + ' ! tensor_sink name=res_object '
        )

        # bus and message callback
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect('message', self.on_bus_message)

        # tensor sink signal : new data callback
        res_face = self.pipeline.get_by_name('res_face')
        res_face.connect('new-data', self.new_callback_face)

        # tensor sink signal : new data callback
        tensor_sink = self.pipeline.get_by_name('res_object')
        tensor_sink.connect('new-data', self.new_callback_object)

        tensor_res = self.pipeline.get_by_name('tensor_res')
        tensor_res.connect('draw', self.draw_overlay_cb)
        tensor_res.connect('caps-changed', self.prepare_overlay_cb)

        # start pipeline
        self.pipeline.set_state(Gst.State.PLAYING)
        self.running = True

        self.set_window_title('img_tensor', 'NNStreamer Multi Model TFLite Example')

        # run main loop
        self.loop.run()

        # quit when received eos or error message
        self.running = False
        self.pipeline.set_state(Gst.State.NULL)

        bus.remove_signal_watch()

    def new_callback_face(self, sink, buffer):
        self.callback_handler(sink, buffer, self.FACE_LABEL_SIZE, self.detected_faces, self.tflite_face_labels)

    def new_callback_object(self, sink, buffer):
        self.callback_handler(sink, buffer, self.OBJECT_LABEL_SIZE, self.detected_objects, self.tflite_object_labels)

    def callback_handler(self, sink, buffer, label_size, detected_result, detected_label):
        if self.running:
            if buffer.n_memory() != 2:
                return False

            # boxes
            mem_boxes = buffer.peek_memory(0)
            result1, info_boxes = mem_boxes.map(Gst.MapFlags.READ)
            if result1:
                assert info_boxes.size == self.BOX_SIZE * self.DETECTION_MAX * 4, "Invalid info_box size"
                decoded_boxes = list(np.fromstring(info_boxes.data, dtype=np.float32))  # decode bytestrings to float list
            
            # detections
            mem_detections = buffer.peek_memory(1)
            result2, info_detections = mem_detections.map(Gst.MapFlags.READ)
            if result2:
                assert info_detections.size == label_size * self.DETECTION_MAX * 4, "Invalid info_detection size"
                decoded_detections = list(np.fromstring(info_detections.data, dtype=np.float32)) # decode bytestrings to float list

            idx = 0
            
            boxes = []
            for _ in range(self.DETECTION_MAX):
                box = []    
                for _ in range(self.BOX_SIZE):
                    box.append(decoded_boxes[idx])
                    idx += 1
                boxes.append(box)

            idx = 0

            detections = []
            for _ in range(self.DETECTION_MAX):
                detection = []    
                for _ in range(label_size):
                    detection.append(decoded_detections[idx])
                    idx += 1
                detections.append(detection)

            self.get_detected_objects(detections, boxes, label_size, detected_result, detected_label)

            mem_boxes.unmap(info_boxes)
            mem_detections.unmap(info_detections)

    def iou(self, A, B):
        x1 = max(A['x'], B['x'])
        y1 = max(A['y'], B['y'])
        x2 = min(A['x'] + A['width'], B['x'] + B['width'])
        y2 = min(A['y'] + A['height'], B['y'] + B['height'])
        w = max(0, (x2 - x1 + 1))
        h = max(0, (y2 - y1 + 1))
        inter = float(w * h)
        areaA = float(A['width'] * A['height'])
        areaB = float(B['width'] * B['height'])
        o = float(inter / (areaA + areaB - inter))
        return o if o >= 0 else 0

    def nms(self, detected, detected_result, detected_label):
        threshold_iou = 0.5
        detected = sorted(detected, key=lambda a: a['prob'])
        boxes_size = len(detected)

        _del = [False for _ in range(boxes_size)]

        for i in range(boxes_size):
            if not _del[i]:
                for j in range(i + 1, boxes_size):
                    if self.iou(detected[i], detected[j]) > threshold_iou:
                        _del[j] = True

        detected_result.clear()

        for i in range(boxes_size):
            if not _del[i]:
                if detected_label[detected[i]["class_id"]][:-1] != 'person':
                    detected_result.append(detected[i])

                if DEBUG:
                    print("==============================")
                    print("LABEL           : {}".format(detected_label[detected[i]["class_id"]]))
                    print("x               : {}".format(detected[i]["x"]))
                    print("y               : {}".format(detected[i]["y"]))
                    print("width           : {}".format(detected[i]["width"]))
                    print("height          : {}".format(detected[i]["height"]))
                    print("Confidence Score: {}".format(detected[i]["prob"]))

    def get_detected_objects(self, detections, boxes, label_size, detected_result, detected_label):
        threshold_score = 0.5
        detected = list()

        for d in range(self.DETECTION_MAX):
            ycenter = boxes[d][0] / self.Y_SCALE * self.tflite_box_priors[2][d] + self.tflite_box_priors[0][d]
            xcenter = boxes[d][1] / self.X_SCALE * self.tflite_box_priors[3][d] + self.tflite_box_priors[1][d]
            h = math.exp(boxes[d][2] / self.H_SCALE) * self.tflite_box_priors[2][d]
            w = math.exp(boxes[d][3] / self.W_SCALE) * self.tflite_box_priors[3][d]

            ymin = ycenter - h / 2.0
            xmin = xcenter - w / 2.0
            ymax = ycenter + h / 2.0
            xmax = xcenter + w / 2.0

            x = xmin * self.MODEL_WIDTH
            y = ymin * self.MODEL_HEIGHT
            width = (xmax - xmin) * self.MODEL_WIDTH
            height = (ymax - ymin) * self.MODEL_HEIGHT

            for c in range(1, label_size):
                score = 1.0 / (1.0 + math.exp(-detections[d][c]))

                # This score cutoff is taken from Tensorflow's demo app.
                # There are quite a lot of nodes to be run to convert it to the useful possibility
                # scores. As a result of that, this cutoff will cause it to lose good detections in
                # some scenarios and generate too much noise in other scenario.

                if score < threshold_score:
                    continue

                obj = {
                    'class_id': c,
                    'x': x,
                    'y': y,
                    'width': width,
                    'height': height,
                    'prob': score
                }

                detected.append(obj)
        
        self.nms(detected, detected_result, detected_label)

    # @brief Store the information from the caps that we are interested in.
    def prepare_overlay_cb(self, overlay, caps):
        self.video_caps = caps

    # @brief Callback to draw the overlay.
    def drawer(self, overlay, context, timestamp, duration, detected, tflite_labels):       
        drawed = 0

        for obj in detected:
            label = tflite_labels[obj['class_id']][:-1]

            x = obj['x'] * self.VIDEO_WIDTH // self.MODEL_WIDTH
            y = obj['y'] * self.VIDEO_HEIGHT // self.MODEL_HEIGHT
            width = obj['width'] * self.VIDEO_WIDTH // self.MODEL_WIDTH
            height = obj['height'] * self.VIDEO_HEIGHT // self.MODEL_HEIGHT
            
            # draw rectangle
            context.rectangle(x, y, width, height)
            context.set_source_rgb(1, 0, 0)
            context.set_line_width(1.5)
            context.stroke()
            context.fill_preserve()

            # draw title
            context.move_to(x + 5, y + 25)
            context.text_path(label)
            context.set_source_rgb(1, 0, 0)
            context.fill_preserve()
            context.set_source_rgb(1, 1, 1)
            context.set_line_width(0.3)
            context.stroke()
            context.fill_preserve()

            drawed += 1
            if drawed >= self.MAX_OBJECT_DETECTION:
                break

    # @brief Callback to draw the overlay.
    def draw_overlay_cb(self, overlay, context, timestamp, duration):
        if self.video_caps == None or not self.running:
            return

        # mutex_lock alternative required
        detected_faces = self.detected_faces
        detected_objects = self.detected_objects
        # mutex_unlock alternative needed
        
        context.select_font_face('Sans', cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        context.set_font_size(20.0)

        self.drawer(overlay, context, timestamp, duration, detected_faces, self.tflite_face_labels)
        self.drawer(overlay, context, timestamp, duration, detected_objects, self.tflite_object_labels)

    def on_bus_message(self, bus, message):
        """
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
        """
        Set window title.
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

