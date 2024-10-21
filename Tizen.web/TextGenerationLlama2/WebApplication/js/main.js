/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file main.js
 * @date 18 October 2024
 * @brief Text Generation Offloading example
 * @author Yelin Jeong <yelini.jeong@samsung.com>
 */

import {
  getNetworkType,
  getIpAddress,
  startHybridService,
  startMessagePort,
  gRemoteServices,
} from "./utils.js";

const serviceName = "llama2c";
let serviceHandle = null;
let tensorsData = null;
let tensorsInfo = null;
let ip = null;

function disposeData() {
  if (tensorsData != null) {
    tensorsData.dispose();
  }

  if (tensorsInfo != null) {
    tensorsInfo.dispose();
  }
}

/**
 * Callback function for pipeline sink listener
 */
function sinkListenerCallback(sinkName, data) {
  const tensorsRetData = data.getTensorRawData(0);
  const decoder = new TextDecoder();
  const output = decoder.decode(tensorsRetData.data);
  const label = document.getElementById("output");
  label.innerText = output;
}

/**
 * Run a pipeline that uses other device's resources
 */
function runClient() {
  const remoteService = gRemoteServices.get(serviceName);
  if (
    !gRemoteServices.has(serviceName) ||
    !Object.prototype.hasOwnProperty.call(remoteService, "ip") ||
    !Object.prototype.hasOwnProperty.call(remoteService, "port")
  ) {
    console.log("No remote service available");
    return;
  }

  const filter =
    "tensor_query_client host=" +
    ip +
    " port=" +
    remoteService["port"] +
    " dest-host=" +
    remoteService["ip"] +
    " dest-port=" +
    remoteService["port"] +
    " timeout=100000";

  const pipelineDescription =
    "appsrc name=srcx ! application/octet-stream ! tensor_converter ! other/tensors,format=flexible,num_tensors=1,types=uint8,framerate=0/1 ! " +
    filter +
    " ! appsink name=sinkx";

  serviceHandle = tizen.ml.pipeline.createPipeline(pipelineDescription);
  serviceHandle.start();
  serviceHandle.registerSinkListener("sinkx", sinkListenerCallback);
}

/**
 * Run a pipeline that uses other device's resources
 */
function inference(input) {
  if (!gRemoteServices.has(serviceName)) {
    console.log("Offloading service is disappeared");
    return;
  }

  const encoder = new TextEncoder();
  const inputBuffer = encoder.encode(input);

  tensorsInfo = new tizen.ml.TensorsInfo();
  tensorsInfo.addTensorInfo("tensor", "UINT8", [input.length, 1, 1, 1]);
  tensorsData = tensorsInfo.getTensorsData();
  tensorsData.setTensorRawData(0, inputBuffer);
  serviceHandle.getSource("srcx").inputData(tensorsData);
}

window.onload = async function () {
  const networkType = await getNetworkType();
  ip = await getIpAddress(networkType);

  startMessagePort();
  startHybridService();

  document.getElementById("start").addEventListener("click", function () {
    runClient();
  });

  document.getElementById("run").addEventListener("click", function () {
    inference(document.getElementById("prompt").value);
  });

  // add eventListener for tizenhwkey
  document.addEventListener("tizenhwkey", function (e) {
    if (e.keyName === "back") {
      try {
        console.log("Pipeline is disposed!!");
        serviceHandle.stop();
        serviceHandle.dispose();

        disposeData();
        tizen.application.getCurrentApplication().exit();
      } catch (ignore) {
        console.log("error " + ignore);
      }
    }
  });
};
