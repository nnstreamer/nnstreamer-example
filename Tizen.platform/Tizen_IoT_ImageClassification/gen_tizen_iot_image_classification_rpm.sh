#!/usr/bin/env bash
##
## @file gen_tizen_iot_image_classification_rpm.sh
## @author Gichan Jang <gichan2.jang@samsung.com>
## @date 28 July 2020
## @brief generate the Tizen-IoT image classification example package
##

git init
git add *
git commit -m 'Initial commit'
gbs build -A aarch64 --include-all --clean
rm -rf .git
