#!/usr/bin/env bash
##
## @file gen_vivante-yolov3-pipeline_rpm.sh
## @author HyoungJoo Ahn <hello.ahn@samsung.com>
## @date Mar 04 2020
## @brief generate the Vivante Capi-pipeline example package
##

git init
git add *
git commit -m'Initial commit'
gbs build -A armv7l --clean --include-all
rm -rf .git
