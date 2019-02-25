# Android GUI NNStreamer Application Examples

In order to deploy NNstreamer on Android devices, you have to execute four steps as following:
 * Build Android full source
 * Build Gstreamer full source based Android rootFS with Cerbero
 * Build NNstreamer full source with ndk-build
 * Build NNstreamer-based test applications


## Build Android full source
We describe how to build Android full source in case that developers have to bring up NNstreamer
on Android 7.0 (Nougat) on Ubuntu 16.04 x86_64 destkop PC.

Install required packages
```bash
sudo apt install adb repo
sudo apt-get update
sudo apt-get install openjdk-8-jdk
sudo update-alternatives --config java
sudo update-alternatives --config javac
```

Download Android full source with 'repo' command.
```bash
mkdir WORKING_DIRECTORY
cd WORKING_DIRECTORY
repo init -u https://android.googlesource.com/platform/manifest -b android-7.0.0_r35   
time repo sync -j$(nproc)

```

Compild Androud full source. From our experience, you need to wait for 60 minutes to compile all source codes.
```bash
make clobber
source build/envsetup.sh
lunch
time make -j$(nproc)
ls -al ./out/
```


## Build Gstreamer full source based Android rootFS with Cerbero
 * TODO 


## Build NNstreamer full source with ndk-build
 * TODO 


## Build NNstreamer full source with ndk-build
 * TODO 


## Build NNstreamer-based test applications
 * TODO 

