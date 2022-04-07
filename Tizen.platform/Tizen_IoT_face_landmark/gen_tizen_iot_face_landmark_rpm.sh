#!/usr/bin/env bash
##
## @file gen_tizen_iot_face_landmark_rpm.sh
## @author Hyunil Park <hyunil46.park.jang@samsung.com>
## @date 8 April 2022
## @brief generate the Tizen-IoT face landmark example package
##

git init
git add *
git commit -m 'Initial commit'
#gbs build -A armv7l --include-all --clean
gbs build -A aarch64 --include-all --clean
rm -rf .git
