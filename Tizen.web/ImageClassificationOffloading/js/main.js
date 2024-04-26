/* SPDX-License-Identifier: Apache-2.0-only */

/**
 * @file main.js
 * @date 30 April 2024
 * @brief Image classification Offloading example
 * @author Yelin Jeong <yelini.jeong@samsung.com>
 * @bug When pushing data to appsrc for the first time
 * after creating a pipeline, the sink listener is not called.
 */

var localSrc;
var remoteSrc;
var labels;
var startTime;

/**
 * Find the index of maximum value in the given array
 * @param array the score list of each class
 * @returns the index of the maximum value
 */
function GetMaxIdx(array) {
    if (array.length === 0) {
        return -1;
    }

    var max = array[0];
    var maxIdx = 0;

    for (var i = 0; i < array.length; ++i) {
        if (array[i] > max) {
            maxIdx = i;
            max = array[i];
        }
    }
    return maxIdx;
}

/**
 * Load the label from the text file and return the string array
 * @returns string array
 */
function loadLabelInfo() {
    var fHandle = tizen.filesystem.openFile("wgt-package/res/labels.txt", 'r');
    var labelList = fHandle.readString();
    return labelList.split('\n');
}

/**
 * Run a pipeline that uses Tizen device's resources
 */
function runLocal() {
    const modelPath = 'wgt-package/res/mobilenet_v1_1.0_224_quant.tflite';
    var URI_PREFIX = 'file://';
    var absModelPath = tizen.filesystem.toURI(modelPath).substr(URI_PREFIX.length);

    var pipelineDescription = "appsrc caps=image/jpeg name=srcx ! jpegdec ! " +
        "videoconvert ! video/x-raw,format=RGB,framerate=0/1,width=224,height=224 ! tensor_converter ! " +
        "tensor_filter framework=tensorflow-lite model=" + absModelPath + " ! " +
        "appsink name=sinkx_local";

    var pHandle = tizen.ml.pipeline.createPipeline(pipelineDescription);
    pHandle.start();

    localSrc = pHandle.getSource('srcx');

    pHandle.registerSinkListener('sinkx_local', function(sinkName, data) {
        var endTime = performance.now();
        var label = document.querySelector('#label');
        var tensorsRetData = data.getTensorRawData(0);
        var maxIdx = GetMaxIdx(tensorsRetData.data);
        label.innerText = labels[maxIdx];

        var time = document.querySelector('#local_time');
        time.innerText = endTime - startTime + " ms"
    });
}

/**
 * Run a pipeline that uses other device's resources
 */
function runRemote() {
    if (document.getElementById('port').value == 0) {
        console.log("No port number is given")
        return
    }

    /* TODO : Only use internal network now */
    var pipelineDescription = "appsrc caps=image/jpeg name=srcx ! jpegdec ! " +
        "videoconvert ! video/x-raw,format=RGB,framerate=0/1,width=224,height=224 ! tensor_converter  ! " +
        "other/tensor,format=static,dimension=(string)3:224:224:1,type=uint8,framerate=0/1  ! " +
        "tensor_query_client host=192.168.50.38 port=" + document.getElementById('port').value + " dest-host=" + "192.168.50.191" + " " +
        "dest-port=" + document.getElementById('port').value + " ! " +
        "other/tensor,format=static,dimension=(string)1001:1,type=uint8,framerate=0/1 ! tensor_sink name=sinkx_remote";

    var pHandle = tizen.ml.pipeline.createPipeline(pipelineDescription);
    pHandle.start();

    remoteSrc = pHandle.getSource('srcx');

    pHandle.registerSinkListener('sinkx_remote', function(sinkName, data) {
        var endTime = performance.now();
        var label = document.querySelector('#label');
        var tensorsRetData = data.getTensorRawData(0);
        var maxIdx = GetMaxIdx(tensorsRetData.data);
        label.innerText = labels[maxIdx];

        var time = document.querySelector('#offloading_time');
        time.innerText = endTime - startTime + " ms"
    });
}

window.onload = function() {
    labels = loadLabelInfo();

    /* TODO : Change the image for each inference */
    var fHandle = tizen.filesystem.openFile("wgt-package/res/0.jpg", 'r');
    var imgUInt8Array = fHandle.readData();
    fHandle.close();

    var tensorsInfo = new tizen.ml.TensorsInfo();
    tensorsInfo.addTensorInfo("tensor", "UINT8", [imgUInt8Array.length]);
    var tensorsData = tensorsInfo.getTensorsData();
    tensorsData.setTensorRawData(0, imgUInt8Array);

    const btnLocal = document.querySelector("#local");

    btnLocal.addEventListener("click", function() {
        runLocal();
    });

    const btnOffloading = document.querySelector("#offloading");

    btnOffloading.addEventListener("click", function() {
        runRemote();
    });

    const btnLocalPush = document.querySelector("#localPush");

    btnLocalPush.addEventListener("click", function() {
        startTime = performance.now()
        localSrc.inputData(tensorsData);
    });

    const btnOffloadingPush = document.querySelector("#offloadingPush");

    btnOffloadingPush.addEventListener("click", function() {
        startTime = performance.now()
        remoteSrc.inputData(tensorsData);
    });

    /* add eventListener for tizenhwkey */
    document.addEventListener('tizenhwkey', function(e) {
        if (e.keyName === "back") {
            try {
                console.log("Pipeline is disposed!!");
                pHandle.stop();
                pHandle.dispose();
                tensorsData.dispose();
                tensorsInfo.dispose();

                tizen.application.getCurrentApplication().exit();
            } catch (ignore) {}
        }
    });
};
