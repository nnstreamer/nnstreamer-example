# Tizen native (c) NNStreamer Example Application

Provides a basic tizen native application to verify that platform libraries can
load shared objects of application. The application contains a neural network
implemented as a custom filter packaged inside its tpk.

The application can be built and run on device/emulator using Tizen Studio.
The applicaiton was tested with Tizen Emulator with Tizen 5.0 mobile version.

Note: use `fetch_shared_object.sh` script to download so file into the `res`
of the application. Set the architecture and device appropriately.
Specify armv7l as the architecture for ARM builds.

Note: use the nnstreamer rpm files from the same build as custom filter. Using
packages from different builds can cause compatibility issues and might not
work together. For ARM devices, the packages can be downloaded from
http://download.tizen.org/snapshots/tizen/unified/latest/repos/standard/packages/armv7l/
