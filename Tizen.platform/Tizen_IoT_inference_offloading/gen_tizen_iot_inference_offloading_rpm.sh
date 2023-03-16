#!/usr/bin/env bash
##
## @file gen_tizen_iot_inference_offloading_rpm.sh
## @author Gichan Jang <gichan2.jang@samsung.com>
## @date 22 Mar 2023
## @brief generate the Tizen-IoT simple inference offloading example package
##

git init
git add *
git commit -m 'Initial commit'
gbs build -A armv7l --include-all --clean
rm -rf .git
