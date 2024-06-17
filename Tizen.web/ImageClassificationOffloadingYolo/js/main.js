/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file main.js
 * @date 17 June 2024
 * @brief Yolo Image classification Offloading example
 * @author Yelin Jeong <yelini.jeong@samsung.com>
 * @bug When drawing the results on the canvas,
 * a blank image appears until the second attempts.
 * (Only on Tizen RPI4 devices)
 */

import {
    getAbsPath,
    getNetworkType,
    getIpAddress,
    GetImgPath,
} from "./utils.js";

let fHandle = null;
let tensorsData = null;
let tensorsInfo = null;

function disposeData() {
    if (fHandle != null) {
        fHandle.close();
    }

    if (tensorsData != null) {
        tensorsData.dispose();
    }

    if (tensorsInfo != null) {
        tensorsInfo.dispose();
    }
}

let localHandle;
let offloadingHandle;

function createPipelineDescription(isLocal, filter) {
    const absLabelPath = getAbsPath("coco.txt");

    return (
        "appsrc caps=image/jpeg name=srcx_" + (isLocal ? "local" : "offloading") + " ! jpegdec ! " +
        "videoconvert ! video/x-raw,format=RGB,width=224,height=224,framerate=0/1 ! tee name=t " +
        "t. ! tensor_converter ! tensor_transform mode=arithmetic option=typecast:float32,div:255.0 ! " +
        "other/tensors,num_tensors=1,format=static,dimensions=(string)3:224:224:1,types=float32,framerate=0/1  ! " +
        filter + " ! " + "other/tensors,num_tensors=1,types=float32,format=static,dimensions=1029:84:1 ! " +
        "tensor_transform mode=transpose option=1:0:2:3 ! " +
        "other/tensors,num_tensors=1,types=float32,format=static,dimensions=84:1029:1 !" +
        "tensor_decoder mode=bounding_boxes option1=yolov8 option2=" + absLabelPath + " option3=0 option4=224:224 option5=224:224 ! " +
        "video/x-raw,width=224,height=224,format=RGBA,framerate=0/1 ! mix.sink_0 t. ! mix.sink_1 " +
        "compositor name=mix sink_0::zorder=2 sink_1::zorder=1 ! video/x-raw,width=224,height=224,format=RGBA,framerate=0/1  ! tensor_converter ! " +
        "other/tensors,num_tensors=1,types=uint8,format=static,dimensions=4:224:224:1 ! " +
        "appsink sync=false name=sinkx_" + (isLocal ? "local" : "offloading")
    );
}

/**
 * Callback function for pipeline sink listener
 */
function sinkListenerCallback(sinkName, data) {
    const endTime = performance.now();

    const tensorsRetData = data.getTensorRawData(0).data;
    const pixelData = new Uint8ClampedArray(tensorsRetData);
    const imageData = new ImageData(pixelData, 224);

    let type;
    if (sinkName.endsWith("local")) {
        type = "local";
    } else {
        type = "offloading";
    }

    const canvas = document.getElementById("canvas_" + type);
    canvas.width = 224;
    canvas.height = 224;
    const ctx = canvas.getContext("2d");
    ctx.putImageData(imageData, 0, 0);

    const time = document.getElementById("time_" + type);
    time.innerText = type + " : " + (endTime - startTime) + " ms";
}

/**
 * Run a pipeline that uses Tizen device's resources
 */
function runLocal() {
    const absModelPath = getAbsPath("yolov8s_float32.tflite");
    const filter =
        "tensor_filter framework=tensorflow-lite model=" + absModelPath;

    const pipelineDescription = createPipelineDescription(true, filter);

    localHandle = tizen.ml.pipeline.createPipeline(pipelineDescription);
    localHandle.start();
    localHandle.registerSinkListener("sinkx_local", sinkListenerCallback);
}

/**
 * Run a pipeline that uses other device's resources
 */
function runOffloading() {
    const filter =
        "tensor_query_client host=" + ip +
        " port=" + document.getElementById("port").value +
        " dest-host=" + document.getElementById("ip").value +
        " dest-port=" + document.getElementById("port").value +
        " timeout=1000";

    const pipelineDescription = createPipelineDescription(false, filter);

    offloadingHandle = tizen.ml.pipeline.createPipeline(pipelineDescription);
    offloadingHandle.start();
    offloadingHandle.registerSinkListener(
        "sinkx_offloading",
        sinkListenerCallback,
    );
}

let startTime;

/**
 * Run a pipeline that uses other device's resources
 */
function inference(isLocal) {
    const img_path = GetImgPath();
    let img = new Image();
    img.src = img_path;

    img.onload = function () {
        disposeData();
        fHandle = tizen.filesystem.openFile("wgt-package" + img_path, "r");
        const imgUInt8Array = fHandle.readData();

        tensorsInfo = new tizen.ml.TensorsInfo();
        tensorsInfo.addTensorInfo("tensor", "UINT8", [imgUInt8Array.length]);
        tensorsData = tensorsInfo.getTensorsData();
        tensorsData.setTensorRawData(0, imgUInt8Array);

        startTime = performance.now();

        if (isLocal) {
        localHandle.getSource("srcx_local").inputData(tensorsData);
        } else {
        offloadingHandle.getSource("srcx_offloading").inputData(tensorsData);
        }
    };
}

let ip;

window.onload = async function () {
    const networkType = await getNetworkType();
    ip = await getIpAddress(networkType);

    document
        .getElementById("start_local")
        .addEventListener("click", function () {
            runLocal();
        });

    document
        .getElementById("start_offloading")
        .addEventListener("click", function () {
            runOffloading();
        });

    document
        .getElementById("local")
        .addEventListener("click", function () {
            inference(true);
        });

    document
        .getElementById("offloading")
        .addEventListener("click", function () {
            inference(false);
        });

    /* add eventListener for tizenhwkey */
    document.addEventListener("tizenhwkey", function (e) {
        if (e.keyName === "back") {
        try {
            console.log("Pipeline is disposed!!");
            localHandle.stop();
            localHandle.dispose();

            offloadingHandle.stop();
            offloadingHandle.dispose();

            disposeData();

            tizen.application.getCurrentApplication().exit();
        } catch (ignore) {}
        }
    });
};
