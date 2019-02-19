#!/usr/bin/env bash

echo -e "Generating Bootstrapping files..."
time ./cerbero-uninstalled  -c config/cross-android-arm64.cbc bootstrap

echo -e "Generating Gstreamer files with NDK r12b for Android software stack..."
time ./cerbero-uninstalled  -c config/cross-android-arm64.cbc package gstreamer-1.0

if [[ $? -eq 0 ]]; then
    echo -e "Congratulations. The all tasks are successfully completed."
else
    echo -e "Ooooops. The some tasks are failed. Please try to run $0 after fixing the issues."
fi
