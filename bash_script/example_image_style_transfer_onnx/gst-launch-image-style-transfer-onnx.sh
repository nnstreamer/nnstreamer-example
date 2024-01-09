#!/usr/bin/env bash
gst-launch-1.0 v4l2src name=cam_src ! videoconvert ! videoscale ! video/x-raw,width=720,height=720,format=RGB ! tee name=t_raw \
       t_raw. ! queue ! videoconvert ! ximagesink name=img_origin sync=false \
       t_raw. ! queue leaky=2 max-size-buffers=10 ! tensor_converter ! tensor_transform mode=transpose option=1:2:0:3 ! \
       tensor_transform mode=arithmetic option=typecast:float32,add:0.0 ! tensor_filter framework=onnxruntime model=onnx_image_style_transfer/candy.onnx ! \
       tensor_converter ! tensor_transform mode=transpose option=2:0:1:3 ! tensor_transform mode=arithmetic option=typecast:uint8,add:0.0 ! \
       tensor_decoder mode=direct_video ! videoconvert ! ximagesink name=candy_img sync=false \
       t_raw. ! queue leaky=2 max-size-buffers=10 ! tensor_converter ! tensor_transform mode=transpose option=1:2:0:3 ! \
       tensor_transform mode=arithmetic option=typecast:float32,add:0.0 ! tensor_filter framework=onnxruntime model=onnx_image_style_transfer/la_muse.onnx ! \
       tensor_converter ! tensor_transform mode=transpose option=2:0:1:3 ! tensor_transform mode=arithmetic option=typecast:uint8,add:0.0 ! \
       tensor_decoder mode=direct_video ! videoconvert ! ximagesink name=la_muse_img sync=false \
       t_raw. ! queue leaky=2 max-size-buffers=10 ! tensor_converter ! tensor_transform mode=transpose option=1:2:0:3 ! \
       tensor_transform mode=arithmetic option=typecast:float32,add:0.0 ! tensor_filter framework=onnxruntime model=onnx_image_style_transfer/mosaic.onnx ! \
       tensor_converter ! tensor_transform mode=transpose option=2:0:1:3 ! tensor_transform mode=arithmetic option=typecast:uint8,add:0.0 ! \
       tensor_decoder mode=direct_video ! videoconvert ! ximagesink name=mosaic_img sync=false \
       t_raw. ! queue leaky=2 max-size-buffers=10 ! tensor_converter ! tensor_transform mode=transpose option=1:2:0:3 ! \
       tensor_transform mode=arithmetic option=typecast:float32,add:0.0 ! tensor_filter framework=onnxruntime model=onnx_image_style_transfer/udnie.onnx ! \
       tensor_converter ! tensor_transform mode=transpose option=2:0:1:3 ! tensor_transform mode=arithmetic option=typecast:uint8,add:0.0 ! \
       tensor_decoder mode=direct_video ! videoconvert ! ximagesink name=udnie_img sync=false \
