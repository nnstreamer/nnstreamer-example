## face_detection
### Introduction

This example passes camera video stream to a neural network using **tensor_filter**. The neural network detects faces of people in input stream. A black box filter is applied to the detected people's face. If there are multiple faces detected, the mechanism is applied to the faces that are not the largest.

Download model for face-detection can be downloded using get-models.sh
