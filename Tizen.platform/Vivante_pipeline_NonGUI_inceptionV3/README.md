# HOW TO USE

To get RPM file, please run below command first.
```bash
$ ./gen_vivante-inceptionv3-pipeline_rpm.sh
```
you can check the rpm files at your ${GBS-ROOT}

After gbs build and install at your target, you can execute it with below command.
```bash
$ vivante-inceptionv3-pipeline ${PATH_TO_NB_FILE} ${PATH_TO_SO_FILE} ${PATH_TO_JPEG}
# e.g. $ vivante-inceptionv3-pipeline \
> /usr/share/dann/inception-v3.nb \
> /usr/share/vivante/inceptionv3/libinceptionv3.so \
> /usr/share/vivante/res/sample_pizza_299x299.jpg
```

The result should be similar with below
```bash
root@localhost:~# vivante-inceptionv3-pipeline \
> /usr/share/dann/inception-v3.nb \
> /usr/share/vivante/inceptionv3/libinceptionv3.so \
> /usr/share/vivante/res/sample_pizza_299x299.jpg 

MODEL_NB path: /usr/share/dann/inception-v3.nb
MODEL_SO path: /usr/share/vivante/inceptionv3/libinceptionv3.so
IMAGE path: /usr/share/vivante/res/sample_pizza_299x299.jpg
[146] pipeline: filesrc location="/usr/share/vivante-inceptionv3-pipeline/sample_pizza_299x299.jpg" blocksize=-1 ! jpegdec ! videoconvert ! video/x-raw,fox
D [setup_node:368]Setup node id[0] uid[0] op[NBG]
D [print_tensor:136]in(0) : id[   1] vtl[0] const[0] shape[ 3, 299, 299, 1   ] fmt[u8 ] qnt[ASM zp=128, scale=0.007812]
D [print_tensor:136]out(0): id[   0] vtl[0] const[0] shape[ 5, 1             ] fmt[f16] qnt[NONE]
D [optimize_node:312]Backward optimize neural network
D [optimize_node:319]Forward optimize neural network
I [compute_node:261]Create vx node
Create Neural Network: 188ms or 0us
I [vsi_nn_PrintGraph:1421]Graph:
I [vsi_nn_PrintGraph:1422]***************** Tensors ******************
D [print_tensor:146]id[   0] vtl[0] const[0] shape[ 5, 1             ] fmt[f16] qnt[NONE]
D [print_tensor:146]id[   1] vtl[0] const[0] shape[ 3, 299, 299, 1   ] fmt[u8 ] qnt[ASM zp=128, scale=0.007812]
I [vsi_nn_PrintGraph:1431]***************** Nodes ******************
I [vsi_nn_PrintNode:159](             NBG)node[0] [in: 1 ], [out: 0 ] [01307cf8]
I [vsi_nn_PrintGraph:1440]******************************************
Data extracted well
datasize 42949672961
max: 15356, labelnum: 0
>>> RESULT LABEL NUMBER: 0
```
