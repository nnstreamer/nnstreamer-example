#!/usr/bin/env python

"""
@file		nnstreamer_example_multi_model_face_pose.py
@date		4 Oct 2020
@brief		Tensor stream example with multi TF-Lite model for face detection and pose estimation.
@see		https://github.com/nnstreamer/nnstreamer-example
@author		Jang Jong Ha <jangjongha.sw@gmail.com>
@bug	    Face masking only works properly when detects one person.

This code is a example of implementing multimodel-ssd using object detection model and face detection model.

Pipeline :
v4l2src -- tee -- videoconvert -- cairooverlay -- ximagesink
            |
            --- videoscale -- videorate -- tee -- tensor_converter -- tensor_transform -- tensor_filter -- tensor_sink
                                            |
                                            -- videoconvert -- tee -- videobox -- videoscale -- tensor_converter -- tensor_transform -- tensor_filter -- tensor_sink
                                                                |
                                                                -- videobox -- videoscale -- tensor_converter -- tensor_transform -- tensor_filter -- tensor_sink
                                                                |
                                                                -- (Whatever you want)

Get model by
$ cd $NNST_ROOT/bin
$ bash get-model.sh face-detection-tflite
$ bash get-model.sh pose-estimation-tflite

nnstreamer_example_multi_model_face_pose.py

Run example :
Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plugin.
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
$ python3 nnstreamer_example_multi_model_face_pose.py

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
OVERLAY_DEBUG = False

class NNStreamerExample:
    """NNStreamer example for face detection and pose estimation."""

    def __init__(self, argv=None):
        self.loop = None
        self.pipeline = None
        self.running = False
        self.video_caps = None

        self.BOX_SIZE = 4
        self.LABEL_SIZE = 2
        self.DETECTION_MAX = 1917
        self.MAX_OBJECT_DETECTION = 2   # Update this parameter every time you update the number of faces to be detected.

        self.Y_SCALE = 10.0
        self.X_SCALE = 10.0
        self.H_SCALE = 5.0
        self.W_SCALE = 5.0

        self.VIDEO_WIDTH = 640
        self.VIDEO_HEIGHT = 480

        self.FACE_MODEL_WIDTH = 300
        self.FACE_MODEL_HEIGHT = 300
        self.POSE_MODEL_WIDTH = 257
        self.POSE_MODEL_HEIGHT = 257

        self.KEYPOINT_SIZE = 17
        self.OUTPUT_STRIDE = 32
        self.GRID_XSIZE = 9
        self.GRID_YSIZE = 9

        self.SCORE_THRESHOLD = 0.7

        self.tflite_face_model = ''
        self.tflite_face_labels = []
        self.tflite_pose_model = ''
        self.tflite_pose_labels = []
        self.tflite_box_priors = []
        self.detected_objects = []
        self.kps = [list(), list(), list(), list(), list()]

        if not self.tflite_init():
            raise Exception

        GObject.threads_init()
        Gst.init(argv)

    def run_example(self):
        """Init pipeline and run example.
        :return: None
        """

        print("Run: NNStreamer example for face detection and pose estimation.")

        # main loop
        self.loop = GObject.MainLoop()

        # init pipeline
        # Due to memory leaks and optimization issues, this code currently only supports 2 people per person.
        self.pipeline = Gst.parse_launch(
            'v4l2src name=cam_src ! videoconvert ! videoscale ! videorate ! '
            'video/x-raw,width=' + str(self.VIDEO_WIDTH) + ',height=' + str(self.VIDEO_HEIGHT) + ',format=RGB,framerate=25/2 ! tee name=t_raw '
            't_raw. ! queue ! videoconvert ! cairooverlay name=tensor_res ! ximagesink name=img_tensor '
            't_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! videorate ! '
            'video/x-raw,width=' + str(self.VIDEO_WIDTH) + ',height=' + str(self.VIDEO_HEIGHT) + ',framerate=10/2 ! tee name=model_handler '
            'model_handler. ! queue leaky=2 max-size-buffers=2 ! videoscale ! '
            'video/x-raw,width=' + str(self.FACE_MODEL_WIDTH) + ',height=' + str(self.FACE_MODEL_HEIGHT) + ' ! '
            'tensor_converter ! tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! '
            'tensor_filter framework=tensorflow-lite model=' + self.tflite_face_model + ' ! tensor_sink name=tensor_sink '
            'model_handler. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! tee name=pose_split '
            'pose_split. ! queue leaky=2 max-size-buffers=2 ! videobox name=object0 ! videoscale ! '
            'video/x-raw,width=' + str(self.POSE_MODEL_WIDTH) + ',height=' + str(self.POSE_MODEL_HEIGHT) + ',format=RGB ! tensor_converter ! '
            'tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! '
            'tensor_filter framework=tensorflow-lite model=' + self.tflite_pose_model + ' ! tensor_sink name=posesink_0 '
            'pose_split. ! queue leaky=2 max-size-buffers=2 ! videobox name=object1 ! videoscale ! '
            'video/x-raw,width=' + str(self.POSE_MODEL_WIDTH) + ',height=' + str(self.POSE_MODEL_HEIGHT) + ',format=RGB ! tensor_converter ! '
            'tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! '
            'tensor_filter framework=tensorflow-lite model=' + self.tflite_pose_model + ' ! tensor_sink name=posesink_1 '
            # 'another_split. ! queue leaky=2 max-size-buffers=2 ! videobox name=object2 ! videoscale ! '
            # 'video/x-raw,width=' + str(self.POSE_MODEL_WIDTH) + ',height=' + str(self.POSE_MODEL_HEIGHT) + ',format=RGB ! tensor_converter ! '
            # 'tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! '
            # 'tensor_filter framework=tensorflow-lite model=' + self.tflite_pose_model + ' ! tensor_sink name=posesink_2 '
       )

        # bus and message callback
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect('message', self.on_bus_message)

        # tensor sink signal : new data callback
        tensor_sink = self.pipeline.get_by_name('tensor_sink')
        tensor_sink.connect('new-data', self.new_data_cb)

        posesink_0 = self.pipeline.get_by_name('posesink_0')
        posesink_0.connect('new-data', self.new_data_pose_cb)

        posesink_1 = self.pipeline.get_by_name('posesink_1')
        posesink_1.connect('new-data', self.new_data_pose_cb)

        # # Uncomment here when you want to add more faces to detect and masking
        # posesink_2 = self.pipeline.get_by_name('posesink_2')
        # posesink_2.connect('new-data', self.new_data_pose_cb)

        tensor_res = self.pipeline.get_by_name('tensor_res')
        tensor_res.connect('draw', self.draw_overlay_cb)
        tensor_res.connect('caps-changed', self.prepare_overlay_cb)

        # start pipeline
        self.pipeline.set_state(Gst.State.PLAYING)
        self.running = True

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
        tflite_face_model = 'detect_face.tflite'
        tflite_face_label = 'labels_face.txt'
        tflite_pose_model = 'posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite'
        tflite_pose_label = 'key_point_labels.txt'
        tflite_box_prior = "box_priors.txt"

        current_folder = os.path.dirname(os.path.abspath(__file__))
        model_folder = os.path.join(current_folder, 'tflite_model')
        pose_folder = os.path.join(current_folder, 'tflite_pose_estimation')

        self.tflite_face_model = os.path.join(model_folder, tflite_face_model)
        if not os.path.exists(self.tflite_face_model):
            logging.error('cannot find tflite model [%s]', self.tflite_face_model)
            return False

        label_path = os.path.join(model_folder, tflite_face_label)
        try:
            with open(label_path, 'r') as label_file:
                for line in label_file.readlines():
                    self.tflite_face_labels.append(line)
        except FileNotFoundError:
            logging.error('cannot find tflite label [%s]', label_path)
            return False

        self.tflite_pose_model = os.path.join(pose_folder, tflite_pose_model)
        if not os.path.exists(self.tflite_pose_model):
            logging.error('cannot find tflite model [%s]', self.tflite_pose_model)
            return False

        label_path = os.path.join(pose_folder, tflite_pose_label)
        try:
            with open(label_path, 'r') as label_file:
                for line in label_file.readlines():
                    self.tflite_pose_labels.append(line)
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

        print('finished to face labels, total [{}]'.format(len(self.tflite_face_labels)))
        print('finished to pose labels, total [{}]'.format(len(self.tflite_pose_labels)))
        print('finished to load box_priors, total [{}]'.format(len(self.tflite_box_priors)))
        return True

    # @brief Callback for tensor sink signal.
    def new_data_cb(self, sink, buffer):
        if self.running:
            if buffer.n_memory() != 2:
                return False

            #  tensor type is float32.
            #  [0] dim of boxes > BOX_SIZE : 1 : DETECTION_MAX : 1 (4:1:1917:1)
            #  [1] dim of labels > LABEL_SIZE : DETECTION_MAX : 1 (91:1917:1)

            # To use boxes and detections in python properly, bytestrings that are based on float32 must be decoded into float list.

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
                assert info_detections.size == self.LABEL_SIZE * self.DETECTION_MAX * 4, "Invalid info_detection size"
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
                for _ in range(self.LABEL_SIZE):
                    detection.append(decoded_detections[idx])
                    idx += 1
                detections.append(detection)

            self.get_detected_objects(detections, boxes)

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

    def nms(self, detected):
        threshold_iou = 0.5
        detected = sorted(detected, key=lambda a: a['prob'])
        boxes_size = len(detected)

        _del = [False for _ in range(boxes_size)]

        for i in range(boxes_size):
            if not _del[i]:
                for j in range(i + 1, boxes_size):
                    if self.iou(detected[i], detected[j]) > threshold_iou:
                        _del[j] = True

        # update result
        self.detected_objects.clear()

        box_idx = 0
        for i in range(boxes_size):
            if not _del[i]:
                self.detected_objects.append(detected[i])

                if DEBUG:
                    print("==============================")
                    print("LABEL           : {}".format(self.tflite_face_labels[detected[i]["class_id"]]))
                    print("x               : {}".format(detected[i]["x"]))
                    print("y               : {}".format(detected[i]["y"]))
                    print("width           : {}".format(detected[i]["width"]))
                    print("height          : {}".format(detected[i]["height"]))
                    print("Confidence Score: {}".format(detected[i]["prob"]))
                
                target_box = self.pipeline.get_by_name('object{}'.format(box_idx))
                x = detected[i]["x"] * self.VIDEO_WIDTH // self.FACE_MODEL_WIDTH
                y = detected[i]["y"] * self.VIDEO_HEIGHT // self.FACE_MODEL_HEIGHT
                width = detected[i]["width"] * self.VIDEO_WIDTH // self.FACE_MODEL_WIDTH
                height = detected[i]["height"] * self.VIDEO_HEIGHT // self.FACE_MODEL_HEIGHT

                target_box.set_property('left', x)
                target_box.set_property('top', y)
                target_box.set_property('right', self.VIDEO_WIDTH - x - width)
                target_box.set_property('bottom', self.VIDEO_HEIGHT - y - height)

                box_idx += 1
                if box_idx >= self.MAX_OBJECT_DETECTION:
                    break

    def get_detected_objects(self, detections, boxes):
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

            x = xmin * self.FACE_MODEL_WIDTH
            y = ymin * self.FACE_MODEL_HEIGHT
            width = (xmax - xmin) * self.FACE_MODEL_WIDTH
            height = (ymax - ymin) * self.FACE_MODEL_HEIGHT

            for c in range(1, self.LABEL_SIZE):
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
        
        self.nms(detected)

    # @brief Callback for tensor sink signal.
    def new_data_pose_cb(self, sink, buffer):
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
            
            # The indexes we really need are 1 and 2. 
            # To handling Eyetracking information more faster, rather than watch all information in result, 
            # We'll only traversal indexes range of 0 ~ 2

            sink_idx = int(sink.name.split("_")[-1])
            self.kps[sink_idx].clear()
            
            for keyPointIdx in (0, 1, 2):
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

                yPosition = (yPosIdx / (self.GRID_YSIZE - 1)) * self.POSE_MODEL_HEIGHT + yOffset
                xPosition = (xPosIdx / (self.GRID_XSIZE - 1)) * self.POSE_MODEL_WIDTH + xOffset

                obj = {
                    'y': yPosition,
                    'x': xPosition,
                    'label': keyPointIdx,
                    'score': highestScore
                }

                self.kps[sink_idx].append(obj)
                        
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
        detected = self.detected_objects
        # mutex_unlock alternative needed
        
        drawed = 0
        context.select_font_face('Sans', cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        context.set_font_size(20.0)

        for idx, obj in enumerate(detected):
            # Eye informations are in 1, 2 index

            # Face indexes
            fx = obj['x'] * self.VIDEO_WIDTH // self.FACE_MODEL_WIDTH
            fy = obj['y'] * self.VIDEO_HEIGHT // self.FACE_MODEL_HEIGHT
            
            # Pose indexes
            lx, ly = self.kps[idx][1]['x'], self.kps[idx][1]['y']
            rx, ry = self.kps[idx][2]['x'], self.kps[idx][2]['y']
            # print(lx, ly, rx, ry)
            
            # Coordinate Conversion
            conv_lx, conv_ly = (lx * (self.FACE_MODEL_WIDTH // self.POSE_MODEL_WIDTH)) + fx, (ly * (self.FACE_MODEL_HEIGHT // self.POSE_MODEL_HEIGHT)) + fy
            conv_rx, conv_ry = (rx * (self.FACE_MODEL_WIDTH // self.POSE_MODEL_WIDTH)) + fx, (ry * (self.FACE_MODEL_HEIGHT // self.POSE_MODEL_HEIGHT)) + fy

            if OVERLAY_DEBUG:
                print(conv_lx, conv_ly, conv_rx, conv_ry)

            width = obj['width'] * self.VIDEO_WIDTH // self.FACE_MODEL_WIDTH
            height = obj['height'] * self.VIDEO_HEIGHT // self.FACE_MODEL_HEIGHT

            # Draw Black line
            context.set_source_rgb(0, 0, 0)
            context.set_line_width(80)
            context.move_to(conv_lx - 150, conv_ly + 5)
            context.line_to(conv_rx + 100, conv_ry + 5)
            context.stroke()
            
            # draw rectangle
            context.rectangle(fx, fy, width, height)
            context.set_source_rgb(1, 0, 0)
            context.set_line_width(1.5)
            context.stroke()
            context.fill_preserve()
    
            drawed += 1
            if drawed >= self.MAX_OBJECT_DETECTION:
                break

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

