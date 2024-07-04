/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file util.js
 * @date 7 June 2024
 * @brief Utility function for Image Classification Offloading
 * @author Yelin Jeong <yelini.jeong@samsung.com>
 */

let gServiceAppId = "EQmf4iSfpX.imageclassificationoffloadingservice";
export let gRemoteServices = new Map();

/**
 * Get currently used network type
 * @returns the network type
 */
export async function getNetworkType() {
  return new Promise((resolve) => {
    tizen.systeminfo.getPropertyValue("NETWORK", function (data) {
      resolve(data.networkType);
    });
  });
}

/**
 * Get IP address of the given network type
 * @param networkType the network type used
 * @returns ip address of the network type
 */
export async function getIpAddress(networkType) {
  return new Promise((resolve) => {
    tizen.systeminfo.getPropertyValue(
      networkType + "_NETWORK",
      function (property) {
        resolve(property.ipAddress);
      },
    );
  });
}

/**
 * Find the index of maximum value in the given array
 * @param array the score list of each class
 * @returns the index of the maximum value
 */
export function GetMaxIdx(array) {
  if (array.length === 0) {
    console.log("array length zero");
    return -1;
  }

  let maxIdx = array.indexOf(Math.max(...array));

  return maxIdx;
}

/**
 * Get the jpeg image path
 * @returns image path
 */
export function GetImgPath() {
  const MAX_IMG_CNT = 2;
  let imgsrc = GetImgPath.count++ % MAX_IMG_CNT;
  imgsrc = imgsrc.toString().concat(".jpg");
  return "/res/".concat(imgsrc);
}
GetImgPath.count = 0;

/**
 * Load the label from the text file and return the string array
 * @returns string array
 */
export function loadLabelInfo() {
  const fHandle = tizen.filesystem.openFile("wgt-package/res/labels.txt", "r");
  const labelList = fHandle.readString();
  return labelList.split("\n");
}

function launchServiceApp() {
  function onSuccess() {
    console.log("Service App launched successfully");
  }

  function onError(err) {
    console.error("Service App launch failed", err);
  }

  try {
    console.log("Launching [" + gServiceAppId + "] ...");
    tizen.application.launch(gServiceAppId, onSuccess, onError);
  } catch (exc) {
    console.error("Exception while launching HybridServiceApp: " + exc.message);
  }
}

function onGetAppsContextSuccess(contexts) {
  let i = 0;
  let appInfo = null;

  for (i = 0; i < contexts.length; i = i + 1) {
    try {
      appInfo = tizen.application.getAppInfo(contexts[i].appId);
    } catch (exc) {
      console.error("Exception while getting application info " + exc.message);
    }

    if (appInfo.id === gServiceAppId) {
      break;
    }
  }

  if (i >= contexts.length) {
    console.log("Service App not found, Trying to launch service app");
    launchServiceApp();
  }
}

function onGetAppsContextError(err) {
  console.error("getAppsContext exc", err);
}

/**
 * Starts obtaining information about applications
 * that are currently running on a device.
 */
export function startHybridService() {
  try {
    tizen.application.getAppsContext(
      onGetAppsContextSuccess,
      onGetAppsContextError,
    );
  } catch (e) {
    console.log("Get AppContext failed, " + e);
  }
}

export function startMessagePort() {
  try {
    const localMessagePort =
      tizen.messageport.requestLocalMessagePort("MESSAGE_PORT");
    localMessagePort.addMessagePortListener(function (data) {
      let remoteService = new Object();
      let serviceName = "";
      for (var i = 0; i < data.length; i++) {
        if (data[i].key == "name") serviceName = data[i].value;
        else remoteService[data[i].key] = data[i].value;
      }
      gRemoteServices.set(serviceName, remoteService);
    });
  } catch (e) {
    console.log("Failed to create local message port " + e.name);
  }
}
