#!/usr/bin/env bash
##
## @file gen_tizen_iot_text_classification_rpm.sh
## @author Gichan Jang <gichan2.jang@samsung.com>
## @date 16 July 2020
## @brief generate the Tizen-IoT capi-single example package
##

git init
git add *
git commit -m 'Initial commit'
gbs build -A aarch64 --include-all --clean
rm -rf .git
