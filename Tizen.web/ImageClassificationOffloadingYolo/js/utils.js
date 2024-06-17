/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file util.js
 * @date 17 June 2024
 * @brief Utility function for Yolo Image Classification Offloading
 * @author Yelin Jeong <yelini.jeong@samsung.com>
 */

/**
 * Get absolute path for file
 */
export function getAbsPath(fileName) {
    const filePath = 'wgt-package/res/' + fileName;
    const URI_PREFIX = 'file://';
    const absPath = tizen.filesystem.toURI(filePath).substr(URI_PREFIX.length);
    return absPath
}

/**
 * Get currently used network type
 * @returns the network type
 */
export async function getNetworkType() {
    return new Promise((resolve, reject) => {
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
    return new Promise((resolve, reject) => {
        tizen.systeminfo.getPropertyValue(
            networkType + "_NETWORK",
            function (property) {
                resolve(property.ipAddress);
            },
        );
    });
}

/**
 * Get the jpeg image path
 * @returns image path
 */
export function GetImgPath() {
    const MAX_IMG_CNT = 2;
    let imgsrc = GetImgPath.count++ % MAX_IMG_CNT;
    imgsrc = imgsrc.toString().concat('.jpg');
    return '/res/'.concat(imgsrc);
}
GetImgPath.count = 0;
