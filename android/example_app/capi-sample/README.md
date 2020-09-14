# NNStreamer Android Example with Native APIs

## Prerequisite

1. Build NNStreamer Native Library with NNFW (ONE) and SNPE

    To run this example, you should build NNStreamer for Android. Please refer [guide to build nnstreamer for android](https://github.com/nnstreamer/nnstreamer/tree/master/api/android) and [the build script](https://github.com/nnstreamer/nnstreamer/blob/master/api/android/build-android-lib.sh).

    ```bash
    $ cd $NNSTREAMER_ROOT
    $ bash ./api/android/build-android-lib.sh --enable_nnfw=yes --enable_snpe=yes
    ```

2. Get the Library

    To build `capi-sample` application of Android example, you have to extract `nnstreamer-native-[DATE].zip` file of into the `src` directory of capi-sample application as below.

    ```bash
    $ cd $NNSTREAMER_ROOT/android_lib
    $ cp nnstreamer-native-[DATE].zip $ANDROID_DEV_ROOT/workspace/nnstreamer-example/android/example_app/capi-sample/src
    $ cd $ANDROID_DEV_ROOT/workspace/nnstreamer-example/android/example_app/capi-sample/src
    $ unzip nnstreamer-native-[DATE].zip
    ```

3. Prepare External Library for NNFW and SNPE

    To run example with NNFW, you should have related external libraries.

    - NNFW

    ```bash
    $ mkdir -p ./capi-sample/src/main/jni/nnfw/ext/arm64-v8a
    $ cp $NNFW_DIRECTORY/lib/* ./capi-sample/src/main/jni/nnfw/ext/arm64-v8a
    ```

    - SNPE

    ```bash
    $ mkdir -p ./capi-sample/src/main/jni/snpe/lib/ext/arm64-v8a
    $ cp $SNPE_DIRECTORY/lib/aarch64-android-clang6.0/lib*dsp*.so ./capi-sample/src/main/jni/snpe/lib/ext/arm64-v8a
    $ cp $SNPE_DIRECTORY/lib/aarch64-android-clang6.0/libhta.so ./capi-sample/src/main/jni/snpe/lib/ext/arm64-v8a
    $ cp $SNPE_DIRECTORY/lib/dsp/libsnpe*.so ./capi-sample/src/main/jni/snpe/lib/ext/arm64-v8a
    ```
