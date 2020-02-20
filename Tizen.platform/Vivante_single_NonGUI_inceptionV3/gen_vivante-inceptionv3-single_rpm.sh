#!/usr/bin/env bash
##
## @file gen_vivante_single_example_rpm.sh
## @author HyoungJoo Ahn <hello.ahn@samsung.com>
## @date Feb 20 2020
## @brief generate the Vivante capi-single example package
##

git init
git add *
git commit -m'Initial commit'
gbs build -A armv7l --include-all
rm -rf .git
