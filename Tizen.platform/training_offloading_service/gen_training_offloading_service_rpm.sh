#!/usr/bin/env bash

git init
git add *
git commit -m 'Initial commit'
#gbs build -A armv7l --include-all --clean
gbs build  --include-all -A armv7l -P public_9.0_arm
rm -rf .git
