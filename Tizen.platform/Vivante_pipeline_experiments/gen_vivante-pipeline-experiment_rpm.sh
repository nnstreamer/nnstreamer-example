#!/usr/bin/env bash
##
## @file gen_vivante-pipeline-experiment_rpm.sh
## @author HyoungJoo Ahn <hello.ahn@samsung.com>
## @date 11 May 2020
## @brief generate the Vivante Capi-pipeline example package
##

git init
git add *
git commit -m'Initial commit'
gbs build -A armv7l
rm -rf .git
