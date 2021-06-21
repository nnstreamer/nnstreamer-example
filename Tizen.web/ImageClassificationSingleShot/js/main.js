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
    if (array.length === 0) {
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

    // load image labels from text file
    var labels = loadLabelInfo();

    // Load the neural network model and create the singleshot handle
    var model = tizen.ml.single.openModel("wgt-package/res/mobilenet_v1_1.0_224_quant.tflite", null, null, "TENSORFLOW_LITE", "ANY");
    console.info("model handle is created.");

    // add eventListener for tizenhwkey
    document.addEventListener('tizenhwkey', function(e) {
        if (e.keyName === "back") {
            try {
                // cleanup the singleshot handle
                model.close();
                console.info("model handle is disposed.");
                
                tizen.application.getCurrentApplication().exit();
            } catch (ignore) {}
        }
    });

    // add click eventListener
    mainPage.addEventListener("click", function() {
        var img = new Image();
        img.src = GetImgPath();

        img.onload = function () {
            // Prepare input TensorsInfo & TensorsData
            var tensorsInfo = new tizen.ml.TensorsInfo();
            tensorsInfo.addTensorInfo("tensor", "UINT8", [224, 224, 3]);
            var tensorsData = tensorsInfo.getTensorsData();
            var rgb = new Uint8Array(224 * 224 * 3);

            // Workaround
            // MobileNet v1 requires RGB format but image object from getImageData() of Canvas 2D API is RGBA format.
            // Because of this reason, an alpha channel should be removed by directly accessing the buffer.
            // Pipeline APIs can easily remove the bothersome code.
            ctx.drawImage(img, 0, 0);
            var rgba = new Uint8Array(ctx.getImageData(0, 0, 224, 224).data.buffer);
            var s = 0, d = 0;
            while (d < 224 * 224 * 3) {
                rgb[d++] = rgba[s++];
                rgb[d++] = rgba[s++];
                rgb[d++] = rgba[s++];
                s++;
            }
            tensorsData.setTensorRawData(0, rgb);

            // Inference &
            var tensorsDataOut = model.invoke(tensorsData);
            var tensorRawData = tensorsDataOut.getTensorRawData(0).data;

            // Find label & update it
            var maxIdx = GetMaxIdx(tensorRawData);
            label.innerText = labels[maxIdx];

            // Cleanup
            tensorsDataOut.dispose();
            tensorsData.dispose();
            tensorsInfo.dispose();
        };
    });
};
