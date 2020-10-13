"""
@file		ezstreamer.py
@date		19 Oct 2020
@brief		Working in Progress
@see		https://github.com/nnstreamer/nnstreamer
@author		Jongha Jang <jangjongha.sw@gmail.com>
@author		Soonbeen Kim <ksb940925@gmail.com>
@bug		This is early build. More optimization and debugging required.

This code is GUI example of using nnstreamer, three neural models, and rtmp streaming.

Get model by
$ cd $NNST_ROOT/bin
$ ./get-model.sh face-detection-tflite
$ ./get-model.sh object-detection-tflite
$ ./get-model.sh pose-estimation-tflite

Run example :
Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plugin.
$ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
$ python3 ezstreamer.py

See https://lazka.github.io/pgi-docs/#Gst-1.0 for Gst API details.

Required model and resources are stored at below link
https://storage.googleapis.com/download.tensorflow.org/models/tflite/posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite
and make text file of key point labels for this model (total 17 key points include nose, left ear, right ankle, etc.)
"""

import sys
import time
import os
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
OVERLAY_DEBUG = False

from PyQt5.QtWidgets import *
from PyQt5 import uic
from PySide.QtCore import QObject, Slot, Signal, QThread

class EZStreamerCore:
    """EZStreamer Core"""

    def __init__(self, argv=None):
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
        self.tflite_object_model = ''
        self.tflite_object_labels = []
        self.tflite_box_priors = []

        self.detected_faces = []
        self.detected_objects = []
        self.kps = [list(), list(), list(), list(), list()]

        self.pattern = None
        self.AUTH_KEY = "null"

        if not self.tflite_init():
            raise Exception
        
        # Base_Pipelines for Each component
        self.BASE_PIPELINE = ('v4l2src name=cam_src ! videoconvert ! videoscale ! '
            'video/x-raw,width=' + str(self.VIDEO_WIDTH) + ',height=' + str(self.VIDEO_HEIGHT) + ',format=RGB ! tee name=t_raw ')
        self.BASE_MODEL_PIPE = ('t_raw. ! queue leaky=2 max-size-buffers=2 ! videoscale ! videorate ! '
            'video/x-raw,width=' + str(self.VIDEO_WIDTH) + ',height=' + str(self.VIDEO_HEIGHT) + ',framerate=10/1 ! tee name=model_handler ')
        self.BASE_OUTPUT_PIPE = ('t_raw. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! cairooverlay name=tensor_res ! tee name=output_handler ')

        # Output component
        self.LOCAL_OUTPUT_PIPE = ('output_handler. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! ximagesink name=output_local ')
        self.RTMP_OUTPUT_PIPE = ('output_handler. ! queue ! videoconvert ! x264enc bitrate=2000 byte-stream=false key-int-max=60 bframes=0 aud=true tune=zerolatency ! '
            'video/x-h264,profile=main ! flvmux streamable=true name=rtmp_mux '
            'rtmp_mux. ! rtmpsink location=rtmp://a.rtmp.youtube.com/live2/x/' + self.AUTH_KEY + ' '
            'alsasrc name=audio_src ! audioconvert ! audio/x-raw,rate=16000,format=S16LE,channels=1 ! voaacenc bitrate=16000 ! rtmp_mux. ')
        
        # SSD Based detection component
        self.SSD_PIPE = ('model_handler. ! queue ! videoscale ! video/x-raw,width=' + str(self.FACE_MODEL_WIDTH) + ',height=' + str(self.FACE_MODEL_HEIGHT) + ' ! tensor_converter ! '
                        'tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tee name=t_ssd ')
        self.OBJECT_DETECT_PIPE = ('t_ssd. ! queue leaky=2 max-size-buffers=3 ! tensor_filter framework=tensorflow-lite model=' + self.tflite_object_model + ' ! tensor_sink name=res_object ')
        self.FACE_DETECT_PIPE = ('t_ssd. ! queue leaky=2 max-size-buffers=3 ! tensor_filter framework=tensorflow-lite model=' + self.tflite_face_model + ' ! tensor_sink name=res_face ')        

        # Pose Detection component for Eye Tracking
        self.EYETRACK_PIPE = ('model_handler. ! queue leaky=2 max-size-buffers=2 ! videoconvert ! tee name=pose_split '
            'pose_split. ! queue leaky=2 max-size-buffers=2 ! videobox name=object0 ! videoflip method=horizontal-flip ! videoscale ! '
            'video/x-raw,width=' + str(self.POSE_MODEL_WIDTH) + ',height=' + str(self.POSE_MODEL_HEIGHT) + ',format=RGB ! tensor_converter ! '
            'tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! '
            'tensor_filter framework=tensorflow-lite model=' + self.tflite_pose_model + ' ! tensor_sink name=posesink_0 '
            'pose_split. ! queue leaky=2 max-size-buffers=2 ! videobox name=object1 ! videoflip method=horizontal-flip ! videoscale ! '
            'video/x-raw,width=' + str(self.POSE_MODEL_WIDTH) + ',height=' + str(self.POSE_MODEL_HEIGHT) + ',format=RGB ! tensor_converter ! '
            'tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! '
            'tensor_filter framework=tensorflow-lite model=' + self.tflite_pose_model + ' ! tensor_sink name=posesink_1 '
            'pose_split. ! queue leaky=2 max-size-buffers=2 ! videobox name=object2 ! videoflip method=horizontal-flip ! videoscale ! '
            'video/x-raw,width=' + str(self.POSE_MODEL_WIDTH) + ',height=' + str(self.POSE_MODEL_HEIGHT) + ',format=RGB ! tensor_converter ! '
            'tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! '
            'tensor_filter framework=tensorflow-lite model=' + self.tflite_pose_model + ' ! tensor_sink name=posesink_2 ')

        self.OPTION_FM = False
        self.OPTION_EM = False
        self.OPTION_OD = False
        self.OPTION_DM = False
        self.OPTION_XV = False
        self.OPTION_RTMP = False

        GObject.threads_init()
        Gst.init(argv)

    def set_option_info(self, mode, value):
        if mode == "FM":
            self.OPTION_FM = value

        if mode == "EM":
            self.OPTION_EM = value

        if mode == "OD":
            self.OPTION_OD = value

        if mode == "XV":
            self.OPTION_XV = value

        if mode == "RTMP":
            self.OPTION_RTMP = value

    def set_rtmp_auth_key(self, value):
        self.AUTH_KEY = value
        self.RTMP_OUTPUT_PIPE = ('output_handler. ! queue ! videoconvert ! x264enc bitrate=2000 byte-stream=false key-int-max=60 bframes=0 aud=true tune=zerolatency ! '
            'video/x-h264,profile=main ! flvmux streamable=true name=rtmp_mux '
            'rtmp_mux. ! rtmpsink location=rtmp://a.rtmp.youtube.com/live2/x/' + self.AUTH_KEY + ' '
            'alsasrc name=audio_src ! audioconvert ! audio/x-raw,rate=16000,format=S16LE,channels=1 ! voaacenc bitrate=16000 ! rtmp_mux. ')

    def get_option_info(self):
        print("Face Masking:", self.OPTION_FM)
        print("Eye Masking:", self.OPTION_EM)
        print("Object Detecting:", self.OPTION_OD)
        print("Local Streaming:", self.OPTION_XV)
        print("RTMP Streaming:", self.OPTION_RTMP)
        print("RTMP AUTH KEY:", self.AUTH_KEY)

    def pipeline_initializer(self):
        res = self.BASE_PIPELINE

        if self.OPTION_OD or self.OPTION_FM or self.OPTION_EM:
            res += self.BASE_MODEL_PIPE
            res += self.SSD_PIPE
            if self.OPTION_OD:
                res += self.OBJECT_DETECT_PIPE

            if self.OPTION_FM or self.OPTION_EM:
                res += self.FACE_DETECT_PIPE
                if self.OPTION_EM:
                    res += self.EYETRACK_PIPE

        if self.OPTION_XV or self.OPTION_RTMP:
            res += self.BASE_OUTPUT_PIPE
            if self.OPTION_XV:
                res += self.LOCAL_OUTPUT_PIPE

            if self.OPTION_RTMP:
                res += self.RTMP_OUTPUT_PIPE

        return res

    def run_example(self):
        print("Run: EZStreamer Core")

        # main loop
        self.loop = GObject.MainLoop()
        pipeline_string = self.pipeline_initializer()

        # init pipeline
        self.pipeline = Gst.parse_launch(
            pipeline_string
        )

        # set mask pattern (for mosaic pattern)
        self.set_mask_pattern()

        # bus and message callback
        bus = self.pipeline.get_bus()
        bus.add_signal_watch()
        bus.connect('message', self.on_bus_message)

        if self.OPTION_FM or self.OPTION_EM:
            res_face = self.pipeline.get_by_name('res_face')
            res_face.connect('new-data', self.new_callback_face)

        if self.OPTION_OD:
            tensor_sink = self.pipeline.get_by_name('res_object')
            tensor_sink.connect('new-data', self.new_callback_object)

        if self.OPTION_EM:
            posesink_0 = self.pipeline.get_by_name('posesink_0')
            posesink_0.connect('new-data', self.new_data_pose_cb)

            posesink_1 = self.pipeline.get_by_name('posesink_1')
            posesink_1.connect('new-data', self.new_data_pose_cb)

            posesink_2 = self.pipeline.get_by_name('posesink_2')
            posesink_2.connect('new-data', self.new_data_pose_cb)

        if self.OPTION_XV or self.OPTION_RTMP:
            tensor_res = self.pipeline.get_by_name('tensor_res')
            tensor_res.connect('draw', self.draw_overlay_cb)
            tensor_res.connect('caps-changed', self.prepare_overlay_cb)

        # start pipeline
        self.pipeline.set_state(Gst.State.PLAYING)
        self.running = True

        if self.OPTION_XV:
            self.set_window_title('output_local', 'EZStreamer Local Streaming')
        
    def stop_stream(self):
        self.running = False
        self.pipeline.set_state(Gst.State.NULL)
        bus = self.pipeline.get_bus()
        bus.remove_signal_watch()

    def tflite_init(self):
        """
        :return: True if successfully initialized
        """
        tflite_face_model = 'detect_face.tflite'
        tflite_face_label = 'labels_face.txt'
        tflite_pose_model = 'posenet_mobilenet_v1_100_257x257_multi_kpt_stripped.tflite'
        tflite_pose_label = 'key_point_labels.txt'
        tflite_object_model = 'ssd_mobilenet_v2_coco.tflite'
        tflite_object_label = 'coco_labels_list.txt'
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

        self.tflite_object_model = os.path.join(model_folder, tflite_object_model)
        if not os.path.exists(self.tflite_object_model):
            logging.error('cannot find tflite model [%s]', self.tflite_object_model)
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

        print('finished to face labels, total [{}]'.format(len(self.tflite_face_labels)))
        print('finished to load object labels, total [{}]'.format(len(self.tflite_object_labels)))
        print('finished to pose labels, total [{}]'.format(len(self.tflite_pose_labels)))
        print('finished to load box_priors, total [{}]'.format(len(self.tflite_box_priors)))
        return True

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

            x = xmin * self.FACE_MODEL_WIDTH
            y = ymin * self.FACE_MODEL_HEIGHT
            width = (xmax - xmin) * self.FACE_MODEL_WIDTH
            height = (ymax - ymin) * self.FACE_MODEL_HEIGHT

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
            
            # The indexes we really needs are 1 and 2. 
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
    def drawer(self, overlay, context, timestamp, duration, detected, tflite_labels):       
        drawed = 0

        face_sizes = [0 for _ in range(len(detected))]

        if len(face_sizes) != 0:
            target_image_idx = face_sizes.index(max(face_sizes))

        for idx, obj in enumerate(detected):
            label = tflite_labels[obj['class_id']][:-1]

            x = obj['x'] * self.VIDEO_WIDTH // self.FACE_MODEL_WIDTH
            y = obj['y'] * self.VIDEO_HEIGHT // self.FACE_MODEL_HEIGHT
            width = obj['width'] * self.VIDEO_WIDTH // self.FACE_MODEL_WIDTH
            height = obj['height'] * self.VIDEO_HEIGHT // self.FACE_MODEL_HEIGHT
            
            # implement pixelated pattern
            if not (len(detected) <= 1 or idx == target_image_idx):
                context.rectangle(x, y, width, height)
                context.set_source(self.pattern)
                context.fill()

            drawed += 1
            if drawed >= self.MAX_OBJECT_DETECTION:
                break

    def eye_mask_drawer(self, overlay, context, timestamp, duration, detected_faces, detected_kps):
        if self.video_caps == None or not self.running:
            return
        
        drawed = 0
        context.select_font_face('Sans', cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        context.set_font_size(20.0)

        for idx, obj in enumerate(detected_faces):
            # Eye informations are in 1, 2 index

            # Face indexes
            fx = obj['x'] * self.VIDEO_WIDTH // self.FACE_MODEL_WIDTH
            fy = obj['y'] * self.VIDEO_HEIGHT // self.FACE_MODEL_HEIGHT
            
            # Pose indexes
            lx, ly = detected_kps[idx][1]['x'], detected_kps[idx][1]['y']
            rx, ry = detected_kps[idx][2]['x'], detected_kps[idx][2]['y']
            
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

    # @brief Callback to draw the overlay.
    def draw_overlay_cb(self, overlay, context, timestamp, duration):
        if self.video_caps == None or not self.running:
            return

        detected_faces = self.detected_faces
        detected_objects = self.detected_objects
        detected_kps = self.kps
        
        context.select_font_face('Sans', cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)
        context.set_font_size(20.0)
        if self.OPTION_FM:
            self.drawer(overlay, context, timestamp, duration, detected_faces, self.tflite_face_labels)

        if self.OPTION_EM:
            self.eye_mask_drawer(overlay, context, timestamp, duration, detected_faces, detected_kps)

        if self.OPTION_OD:
            self.drawer(overlay, context, timestamp, duration, detected_objects, self.tflite_object_labels)

    def set_mask_pattern(self):
        """
        Prepare mask pattern for cairooverlay.
        """
        source = cairo.ImageSurface.create_from_png('./mosaic.png')
        self.pattern = cairo.SurfacePattern(source)
        self.pattern.set_extend(cairo.Extend.REPEAT)

    def on_bus_message(self, bus, message):
        """
        :param bus: pipeline bus
        :param message: message from pipeline
        :return: None
        """
        if message.type == Gst.MessageType.EOS:
            print("Window Closed")
            logging.info('received eos message')
            self.loop.quit()
        elif message.type == Gst.MessageType.ERROR:
            error, debug = message.parse_error()
            if error.message == 'Output window was closed':
                self.stop_stream()
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

class EZStreamerWindow(QMainWindow):
    def __init__(self):
        super(EZStreamerWindow, self).__init__()
        uic.loadUi('ezstreamer.ui', self)
        
        # Get Widget info
        self.stream_button = self.findChild(QPushButton, "start_stop_button")
        self.exit_button = self.findChild(QPushButton, "exit_button")
        self.rtmp_check = self.findChild(QCheckBox, "rtmp_check")
        self.auth_inputbox = self.findChild(QLineEdit, "auth_input")
        self.nothing_radio = self.findChild(QRadioButton, "nothing_radio")
        self.em_radio = self.findChild(QRadioButton, "em_radio")
        self.fm_radio = self.findChild(QRadioButton, "fm_radio")
        self.lo_check = self.findChild(QCheckBox, "lo_check")
        self.od_check = self.findChild(QCheckBox, "od_check")

        # Set Event Listener
        self.stream_button.clicked.connect(self.StreamController)
        self.exit_button.clicked.connect(self.QuitProgram)
        self.rtmp_check.clicked.connect(self.CheckRTMPEnabled)

        # Initialize ezstreamer_core
        self.ezstreamer_core = EZStreamerCore(sys.argv[1:])

    def run_stream(self):
        if self.nothing_radio.isChecked():
            self.ezstreamer_core.set_option_info("FM", False)
            self.ezstreamer_core.set_option_info("EM", False)
        else:
            if self.fm_radio.isChecked():
                self.ezstreamer_core.set_option_info("FM", True)
                self.ezstreamer_core.set_option_info("EM", False)
            elif self.em_radio.isChecked():
                self.ezstreamer_core.set_option_info("FM", False)
                self.ezstreamer_core.set_option_info("EM", True)
        
        if self.od_check.isChecked():
            self.ezstreamer_core.set_option_info("OD", True)

        else:
            self.ezstreamer_core.set_option_info("OD", False)

        if self.lo_check.isChecked():
            self.ezstreamer_core.set_option_info("XV", True)

        else:
            self.ezstreamer_core.set_option_info("XV", False)


        if self.rtmp_check.isChecked():
            self.ezstreamer_core.set_option_info("RTMP", True)
            self.ezstreamer_core.set_rtmp_auth_key(self.auth_inputbox.text())

        else:
            self.ezstreamer_core.set_option_info("RTMP", False)

        self.ezstreamer_core.get_option_info()
        self.ezstreamer_core.run_example()

    def stop_stream(self):
        self.ezstreamer_core.stop_stream()

    def toggle_control(self):
        if self.stream_button.text() == "STOP":
            self.auth_inputbox.setEnabled(False)
            self.nothing_radio.setEnabled(False)
            self.em_radio.setEnabled(False)
            self.fm_radio.setEnabled(False)
            self.rtmp_check.setEnabled(False)
            self.lo_check.setEnabled(False)
            self.od_check.setEnabled(False)

        elif self.stream_button.text() == "START":
            self.nothing_radio.setEnabled(True)
            self.em_radio.setEnabled(True)
            self.fm_radio.setEnabled(True)
            self.rtmp_check.setEnabled(True)
            self.lo_check.setEnabled(True)
            self.od_check.setEnabled(True)

            if self.rtmp_check.isChecked():
                self.auth_inputbox.setEnabled(True)

    @Slot()
    def CheckRTMPEnabled(self):
        if self.rtmp_check.isChecked():
            self.auth_inputbox.setEnabled(True)
        else:
            self.auth_inputbox.setEnabled(False)

    @Slot()
    def StreamController(self):
        if self.stream_button.text() == "START":
            self.stream_button.setText("STOP")
            self.toggle_control()
            self.run_stream()

        elif self.stream_button.text() == "STOP":
            self.stream_button.setText("START")
            self.toggle_control()
            self.stop_stream()
    
    @Slot()
    def QuitProgram(self):
        self.close()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    myWindow = EZStreamerWindow()
    myWindow.show()
    app.exec_()

