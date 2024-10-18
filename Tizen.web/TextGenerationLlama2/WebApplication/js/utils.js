/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file utils.js
 * @date 18 October 2024
 * @brief Utility function for Image Classification Offloading
 * @author Yelin Jeong <yelini.jeong@samsung.com>
 */

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
