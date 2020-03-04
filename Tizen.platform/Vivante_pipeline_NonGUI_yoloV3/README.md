# HOW TO USE

To get RPM file, please run below command first.
```bash
$ ./gen_vivante-yolov3-pipeline_rpm.sh
```
you can check the rpm files at your ${GBS-ROOT}

If you want to generate the *.bin file with your image, please follow bellow pipeline.
```bash
gst-launch-1.0 filesrc location=${IMAGE_PATH} ! \
jpegdec ! videoconvert ! video/x-raw,format=BGR,width=416,height=416 ! \
tensor_converter ! tensor_transform mode=transpose option=1:2:0:3 ! \
tensor_transform mode=arithmetic option=div:2 ! \
tensor_transform mode=typecast option=int8 ! \
tensor_filter framework=vivante model="/usr/share/dann/yolo-v3.nb,/usr/share/vivante/yolov3/libyolov3.so" ! \
filesink location=${BIN_FILE_PATH}
```

After gbs build and install at your target, you can execute it with below command.
```bash
root@localhost:~/rpms# vivante-yolov3-pipeline \
> /usr/share/dann/yolo-v3.nb \
> /usr/share/vivante/yolov3/libyolov3.so \
> /usr/share/vivante/yolov3/sample_car_bicyle_dog_416x416.jpg \
> /usr/share/vivante/res/sample_car_bicyle_dog_416x416.bin 
```
```
MODEL_NB path: /usr/share/dann/yolo-v3.nb
MODEL_SO path: /usr/share/vivante/yolov3/libyolov3.so
IMAGE path: /usr/share/vivante/yolov3/sample_car_bicyle_dog_416x416.jpg
BIN path: /usr/share/vivante/res/sample_car_bicyle_dog_416x416.bin
[203] pipeline: filesrc location="/usr/share/vivante/yolov3/sample_car_bicyle_dog_416x416.jpg" ! jpegdec ! videoconvert ! video/x-raw,format=BGR,width=416,height=**416** ! tensor_converter ! tensor_transform mode=transpose option=1:2:0:3 ! tensor_transform mode=arithmetic option=div:2 ! tensor_transform mode=typecast option=int8 ! tensor_filter framework=vivante model="/usr/share/dann/yolo-v3.nb,/usr/share/vivante/yolov3/libyolov3.so" ! tensor_sink name=sinkx
D [setup_node:368]Setup node id[0] uid[0] op[NBG]
D [print_tensor:136]in(0) : id[   0] vtl[0] const[0] shape[ 416, 416, 3, 1   ] fmt[i8 ] qnt[DFP fl=  7]
D [print_tensor:136]out(0): id[   1] vtl[0] const[0] shape[ 13, 13, 255, 1   ] fmt[i8 ] qnt[DFP fl=  2]
D [print_tensor:136]out(1): id[   2] vtl[0] const[0] shape[ 26, 26, 255, 1   ] fmt[i8 ] qnt[DFP fl=  2]
D [print_tensor:136]out(2): id[   3] vtl[0] const[0] shape[ 52, 52, 255, 1   ] fmt[i8 ] qnt[DFP fl=  2]
D [optimize_node:312]Backward optimize neural network
D [optimize_node:319]Forward optimize neural network
I [compute_node:261]Create vx node
Create Neural Network: 39ms or 0us
I [vsi_nn_PrintGraph:1421]Graph:
I [vsi_nn_PrintGraph:1422]***************** Tensors ******************
D [print_tensor:146]id[   0] vtl[0] const[0] shape[ 416, 416, 3, 1   ] fmt[i8 ] qnt[DFP fl=  7]
D [print_tensor:146]id[   1] vtl[0] const[0] shape[ 13, 13, 255, 1   ] fmt[i8 ] qnt[DFP fl=  2]
D [print_tensor:146]id[   2] vtl[0] const[0] shape[ 26, 26, 255, 1   ] fmt[i8 ] qnt[DFP fl=  2]
D [print_tensor:146]id[   3] vtl[0] const[0] shape[ 52, 52, 255, 1   ] fmt[i8 ] qnt[DFP fl=  2]
I [vsi_nn_PrintGraph:1431]***************** Nodes ******************
I [vsi_nn_PrintNode:159](             NBG)node[0] [in: 0 ], [out: 1, 2, 3 ] [01d18be8]
I [vsi_nn_PrintGraph:1440]******************************************
g_usleep [2000000]: wait for the model result
out_size1 43095
out_size2 172380
out_size3 689520
Result comparision for 13x13x255 layer has been done successfully.
Result comparision for 26x26x255 layer has been done successfully.
Result comparision for 52x52x255 layer has been done successfully.

```
