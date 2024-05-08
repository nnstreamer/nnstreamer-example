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
var ctx;
var label;

/**
 * Find the index of maximum value in the given array
 * @param array the score list of each class
 * @returns the index of the maximum value
 */
function GetMaxIdx(array) {
    if (array.length === 0) {
        console.log('array length zero')
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
 * Get the jpeg image path
 * @returns image path
 */
function GetImgPath() {
    const MAX_IMG_CNT = 2;
    var imgsrc = GetImgPath.count++ % MAX_IMG_CNT;
    imgsrc = imgsrc.toString().concat('.jpg');
    return '/res/'.concat(imgsrc);
}
GetImgPath.count = 0;

/**
 * Load the label from the text file and return the string array
 * @returns string array
 */
function loadLabelInfo() {
    var fHandle = tizen.filesystem.openFile('wgt-package/res/labels.txt', 'r');
    var labelList = fHandle.readString();
    return labelList.split('\n');
}

/**
 * Run a pipeline that uses Tizen device's resources
 */
function runLocal() {
    const modelPath = 'wgt-package/res/mobilenet_v1_1.0_224_quant.tflite';
    const URI_PREFIX = 'file://';
    const absModelPath = tizen.filesystem.toURI(modelPath).substr(URI_PREFIX.length);

    const pipelineDescription = 'appsrc caps=image/jpeg name=srcx_local ! jpegdec ! ' +
        'videoconvert ! video/x-raw,format=RGB,framerate=0/1,width=224,height=224 ! tensor_converter ! ' +
        'tensor_filter framework=tensorflow-lite model=' + absModelPath + ' ! ' +
        'appsink name=sinkx_local';

    const pHandle = tizen.ml.pipeline.createPipeline(pipelineDescription);
    pHandle.start();

    localSrc = pHandle.getSource('srcx_local');

    pHandle.registerSinkListener('sinkx_local', function(sinkName, data) {
        const endTime = performance.now();
        const label = document.getElementById('label_local')
        const tensorsRetData = data.getTensorRawData(0);
        const maxIdx = GetMaxIdx(tensorsRetData.data);
        label.innerText = labels[maxIdx];

        const time = document.getElementById('time_local');
        time.innerText = 'local : ' + (endTime - startTime) + ' ms'
    });
}

/**
 * Run a pipeline that uses other device's resources
 */
function runRemote() {
    if (document.getElementById('port').value == 0) {
        console.log('No port number is given')
        return
    }

    /* TODO : Only use internal network now */
    const pipelineDescription = 'appsrc caps=image/jpeg name=srcx_remote ! jpegdec ! ' +
        'videoconvert ! video/x-raw,format=RGB,framerate=0/1,width=224,height=224 ! tensor_converter  ! ' +
        'other/tensor,format=static,dimension=(string)3:224:224:1,type=uint8,framerate=0/1  ! ' +
        'tensor_query_client host=192.168.50.38 port=' + document.getElementById('port').value + ' dest-host=' + '192.168.50.191' + ' ' +
        'dest-port=' + document.getElementById('port').value + ' ! ' +
        'other/tensor,format=static,dimension=(string)1001:1,type=uint8,framerate=0/1 ! tensor_sink name=sinkx_remote';

    const pHandle = tizen.ml.pipeline.createPipeline(pipelineDescription);
    pHandle.start();

    remoteSrc = pHandle.getSource('srcx_remote');

    pHandle.registerSinkListener('sinkx_remote', function(sinkName, data) {
        const endTime = performance.now();
        const label = document.getElementById('label_offloading');
        const tensorsRetData = data.getTensorRawData(0);
        const maxIdx = GetMaxIdx(tensorsRetData.data);
        label.innerText = labels[maxIdx];

        const time = document.getElementById('time_offloading');
        time.innerText = 'offloading : ' + (endTime - startTime) + ' ms'
    });
}

function inference(src, canvas) {
    const img_path = GetImgPath();
    let img = new Image();
    img.src = img_path;

    img.onload = function () {
        const fHandle = tizen.filesystem.openFile('wgt-package' + img_path, 'r');
        const imgUInt8Array = fHandle.readData();
        fHandle.close();

        const tensorsInfo = new tizen.ml.TensorsInfo();
        tensorsInfo.addTensorInfo('tensor', 'UINT8', [imgUInt8Array.length]);
        const tensorsData = tensorsInfo.getTensorsData();
        tensorsData.setTensorRawData(0, imgUInt8Array);

        startTime = performance.now()
        src.inputData(tensorsData);

        tensorsData.dispose();
        tensorsInfo.dispose();

        const ctx = canvas.getContext('2d');
	    ctx.drawImage(img, 0, 0);
    }
}

window.onload = function() {
    labels = loadLabelInfo();

    const btnLocal = document.getElementById('start_local');

    btnLocal.addEventListener('click', function() {
        runLocal();
    });

    const btnOffloading = document.getElementById('start_offloading');

    btnOffloading.addEventListener('click', function() {
        runRemote();
    });

    const localPage = document.getElementById('local');

    localPage.addEventListener('click', function() {
        if (localSrc) {
            inference(localSrc, document.getElementById('canvas_local'));
        }
    });

    const offloadingPage = document.getElementById('offloading');

    offloadingPage.addEventListener('click', function() {
        if (remoteSrc) {
            inference(remoteSrc, document.getElementById('canvas_offloading'));
        }
    });

    /* add eventListener for tizenhwkey */
    document.addEventListener('tizenhwkey', function(e) {
        if (e.keyName === 'back') {
            try {
                console.log('Pipeline is disposed!!');
                pHandle.stop();
                pHandle.dispose();

                tizen.application.getCurrentApplication().exit();
            } catch (ignore) {}
        }
    });
};
