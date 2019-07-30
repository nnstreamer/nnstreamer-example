#!/usr/bin/env bash

##
## @file fetch_shared_object.sh
## @author Parichay Kapoor <pk.kapoor@samsung.com>
## @date May 15th 2019
## @brief fetch shared object of custom filter
##


# arch should be one of aarch64/x86_64/i686/armv7l for standard device
# arch should be one of x86_64/i686 for emulator device
arch="i686"
if [ $# -ge 1 ]; then
  arch=$1
fi

# arch should be one of standard/emulator
device="emulator"
if [ $# -ge 2 ]; then
  device=$2
fi

# fix the version number and file to get
version="0.2.1-5.1"
rpmfile="nnstreamer-custom-filter-example-$version.$arch.rpm"
sofile="libnnstreamer_customfilter_passthrough.so"

# remove the file if existing
if [ -f $rpmfile ]; then
  rm $rpmfile
fi
wget "http://download.tizen.org/snapshots/tizen/unified/latest/repos/$device/packages/$arch/$rpmfile"

# extract the so from the rpm file and move to correct location
sofilepath=$(rpm2cpio $rpmfile | cpio -t | grep $sofile)
rpm2cpio $rpmfile | cpio -id $sofilepath
chmod 755 $sofilepath
mv -f $sofilepath ./res/.


# clean the remaining files
rm -rf ./usr
rm -rf $rpmfile
