/* SPDX-License-Identifier: Apache-2.0 */

/**
 * @file util.js
 * @date 7 June 2024
 * @brief Utility function for Image Classification Offloading
 * @author Yelin Jeong <yelini.jeong@samsung.com>
 */

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
 * Find the index of maximum value in the given array
 * @param array the score list of each class
 * @returns the index of the maximum value
 */
export function GetMaxIdx(array) {
    if (array.length === 0) {
        console.log('array length zero')
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
    imgsrc = imgsrc.toString().concat('.jpg');
    return '/res/'.concat(imgsrc);
}
GetImgPath.count = 0;

/**
 * Load the label from the text file and return the string array
 * @returns string array
 */
export function loadLabelInfo() {
    const fHandle = tizen.filesystem.openFile('wgt-package/res/labels.txt', 'r');
    const labelList = fHandle.readString();
    return labelList.split('\n');
}
