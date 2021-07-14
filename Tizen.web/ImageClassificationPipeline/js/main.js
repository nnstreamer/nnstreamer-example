/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * Find the index of maximum value in the given array
 * @param array the score list of each class
 * @returns the index of the maximum value
 */
function GetMaxIdx(array) {
    if (array.length === 0){
        return -1;
    }

    var max = array[0];
    var maxIdx = 0;

    for (var i = 0; i< array.length; ++i) {
        if (array[i] > max) {
            maxIdx = i;
            max = array[i];
        }
    }
    return maxIdx;
}

/**
 * Get the jpeg image path
 * @returns image path
 */
function GetImgPath() {
    const MAX_IMG_CNT = 2;
    if (typeof GetImgPath.count === 'undefined') {
        GetImgPath.count = 0;
    }
    var imgsrc = GetImgPath.count++ % MAX_IMG_CNT;
    imgsrc = imgsrc.toString().concat(".jpg");
    return "/res/".concat(imgsrc);
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

window.onload = function() {
    var mainPage = document.querySelector('#main');
    var label = document.querySelector('#label');
    var canvas = document.querySelector('#canvas');
    var ctx = canvas.getContext('2d');

    // To create the pipeline, the model path should be absoulte path.
    const modelPath = 'wgt-package/res/mobilenet_v1_1.0_224_quant.tflite';
    var URI_PREFIX = 'file://';
    var absModelPath = tizen.filesystem.toURI(modelPath).substr(URI_PREFIX.length);

    // load image labels from text file
    var labels = loadLabelInfo();

    // make the pipeline & start it
    var pipelineDescription = "appsrc caps=image/jpeg name=srcx ! jpegdec ! " +
        "videoconvert ! video/x-raw,format=RGB,framerate=0/1,width=224,height=224 ! tensor_converter ! " +
        "tensor_filter framework=tensorflow1-lite model=" + absModelPath + " ! " +
        "appsink name=sinkx";
    var pHandle = tizen.ml.pipeline.createPipeline(pipelineDescription);
    console.log("Pipeline is created!!");
    pHandle.start();

    // get source element
    var srcElement = pHandle.getSource('srcx');

    // register appsink callback
    pHandle.registerSinkListener('sinkx', function(sinkName, data) {
        // update label
        var tensorsRetData = data.getTensorRawData(0);
        var maxIdx = GetMaxIdx(tensorsRetData.data);
        label.innerText = labels[maxIdx];
    });

    // add eventListener for tizenhwkey
    document.addEventListener('tizenhwkey', function(e) {
        if (e.keyName === "back") {
            try {
                // stop the pipeline and cleanup its handles
                console.log("Pipeline is disposed!!");
                pHandle.stop();
                pHandle.dispose();

                tizen.application.getCurrentApplication().exit();
            } catch (ignore) {}
        }
    });

    mainPage.addEventListener("click", function() {
        var img_path = GetImgPath();
        var img = new Image();
        img.src = img_path;

        img.onload = function () {
            // load image file
            var fHandle = tizen.filesystem.openFile("wgt-package/" + img_path, 'r');
            var imgUInt8Array = fHandle.readData();
            fHandle.close();

            // make tensor input data
            var tensorsInfo = new tizen.ml.TensorsInfo();
            tensorsInfo.addTensorInfo("tensor", "UINT8", [imgUInt8Array.length]);
            var tensorsData = tensorsInfo.getTensorsData();
            tensorsData.setTensorRawData(0, imgUInt8Array);

            // input data
            srcElement.inputData(tensorsData);

            // cleanup
            tensorsData.dispose();
            tensorsInfo.dispose();

            // Draw preview image
            ctx.drawImage(img, 0, 0);
        };
    });
};
