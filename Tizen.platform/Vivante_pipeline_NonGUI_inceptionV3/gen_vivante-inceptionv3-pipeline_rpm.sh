#!/usr/bin/env bash
##
## @file gen_vivante_pipeline_example_rpm.sh
## @author HyoungJoo Ahn <hello.ahn@samsung.com>
## @date Feb 19 2020
## @brief generate the Vivante Capi-pipeline example package
##

git init
git add *
git commit -m'Initial commit'
gbs build -A armv7l --include-all
rm -rf .git
