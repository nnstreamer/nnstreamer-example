#!/usr/bin/env bash

git init
git add *
git commit -m 'Initial commit'
gbs build -A armv7l --include-all --clean
rm -rf .git
