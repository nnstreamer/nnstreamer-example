#!/usr/bin/env python

"""
@file		get_tflite_model.py
@date		24 Jun 2024
@brief		get yolov8 tflite model for Tizen Web application
@author		Yelin Jeong <yelini.jeong@samsung.com>
@bug		No known bugs.

"""

from ultralytics import YOLO

# Load a model
model = YOLO("yolov8s.pt") # load a pretrained model

# Export the model
model.export(format="tflite", imgsz=224) # export the model to tflite format
