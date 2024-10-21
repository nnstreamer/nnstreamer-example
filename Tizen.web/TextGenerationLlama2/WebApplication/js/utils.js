/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file utils.js
 * @date 18 October 2024
 * @brief Utility function for Image Classification Offloading
 * @author Yelin Jeong <yelini.jeong@samsung.com>
 */

let gServiceAppId = "lfebC6UrMY.nsdservice";
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
    const addService =
      tizen.messageport.requestLocalMessagePort("STATE_AVAILABLE");
    addService.addMessagePortListener(function (data) {
      let remoteService = new Object();
      let serviceName = "";
      for (var i = 0; i < data.length; i++) {
        if (data[i].key == "name") {
          var name = data[i].value.split(" ")[0];
          serviceName = name;
        } else remoteService[data[i].key] = data[i].value;
      }
      if (gRemoteServices.has(serviceName)) {
        gRemoteServices.delete(serviceName);
      }
      gRemoteServices.set(serviceName, remoteService);
    });

    const removeService =
      tizen.messageport.requestLocalMessagePort("STATE_UNAVAILABLE");
    removeService.addMessagePortListener(function (data) {
      let serviceName = "";
      for (var i = 0; i < data.length; i++) {
        if (data[i].key == "name") {
          var name = data[i].value.split("(")[0];
          serviceName = name;
        }
      }
      gRemoteServices.delete(serviceName);
    });
  } catch (e) {
    console.log("Failed to create local message port " + e.name);
  }
}
