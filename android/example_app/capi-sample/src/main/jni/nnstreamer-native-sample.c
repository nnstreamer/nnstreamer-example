/**
 * @file	nnstreamer-native-sample.c
 * @date	1 November 2019
 * @brief	Sample code with NNStreamer
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 * @bug		No known bugs except for NYI items
 *
 * NNStreamer API sample code for native build.
 *
 * Before running this example, you should build NNStreamer Android library
 * and extract nnstreamer-native.zip into src directory.
 */

#include <jni.h>
#include <unistd.h>
#include <android/log.h>

#include "nnstreamer.h"

#define TAG_NAME "NNStreamer-Sample"
#define LOGD(...) __android_log_print (ANDROID_LOG_DEBUG, TAG_NAME, __VA_ARGS__)
#define LOGI(...) __android_log_print (ANDROID_LOG_INFO, TAG_NAME, __VA_ARGS__)
#define LOGE(...) __android_log_print (ANDROID_LOG_ERROR, TAG_NAME, __VA_ARGS__)

/**
 * @brief Initialize NNStreamer defined in NNStreamer module.
 */
extern jboolean nnstreamer_native_initialize (void);

/**
 * @brief Callback to get the tensor data from sink element.
 */
static void
sample_sink_cb (const ml_tensors_data_h data, const ml_tensors_info_h info, void *user_data)
{
  int status;
  unsigned int i;
  unsigned int count = 0;

  status = ml_tensors_info_get_count (info, &count);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get the number of tensors.");
    return;
  }

  for (i = 0; i < count; i++) {
    void *data_ptr;
    size_t data_size;

    status = ml_tensors_data_get_tensor_data (data, i, &data_ptr, &data_size);
    if (status != ML_ERROR_NONE) {
      LOGE ("Failed to get the tensor data.");
      break;
    }

    /* Do something with received data. */
    LOGD ("[%d] Received size %zd", i, data_size);
  }
}

/**
 * @brief Simple pipeline example.
 */
static void
run_simple_pipeline (void)
{
  ml_pipeline_h pipe = NULL;
  ml_pipeline_sink_h sink = NULL;
  int status = ML_ERROR_NONE;

  /* pipeline description */
  const char description[] = "videotestsrc ! videoconvert ! video/x-raw,format=RGB ! tensor_converter ! tensor_sink name=sinkx";

  LOGI ("Start to run NNStreamer pipeline example.");

  /* construct a pipeline */
  status = ml_pipeline_construct (description, NULL, NULL, &pipe);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to construct the pipeline.");
    goto done;
  }

  /* register sink callback */
  status = ml_pipeline_sink_register (pipe, "sinkx", sample_sink_cb, NULL, &sink);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to construct the pipeline.");
    goto done;
  }

  /* start the pipeline */
  status = ml_pipeline_start (pipe);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to start the pipeline.");
    goto done;
  }

  /* sleep to run pipeline for a while */
  sleep (5);

done:
  /* destroy the pipeline */
  if (ml_pipeline_destroy (pipe) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy pipeline handle.");
  }
}

/**
 * @brief SNAP example.
 */
static void
run_snap_example (void)
{
  ml_pipeline_h pipe = NULL;
  ml_pipeline_sink_h sink = NULL;
  int status = ML_ERROR_NONE;

  /**
   * Pipeline with SNAP
   *
   * Set custom properties in tensor-filter.
   *
   * GPU: "custom=ModelFWType:CAFFE,ExecutionDataType:FLOAT32,ComputingUnit:GPU,GpuCacheSource:/sdcard/nnstreamer/"
   * Denote GPU cache path. (GpuCacheSource:cache-path)
   * It takes a time with GPU option in the first run.
   *
   * CPU: "custom=ModelFWType:CAFFE,ExecutionDataType:FLOAT32,ComputingUnit:CPU"
   * Set CPU thread count. (default 4. e.g., CpuThreadCount:5 to set the count 5.)
   */
  const char description[] = "videotestsrc ! videoconvert ! videoscale ! "
            "video/x-raw,format=RGB,width=224,height=224,framerate=(fraction)30/1 ! "
            "tensor_converter ! tensor_transform mode=typecast option=float32 ! "
            "tensor_filter framework=snap "
                "model=/sdcard/nnstreamer/snap_data/prototxt/squeezenet.prototxt,"
                    "/sdcard/nnstreamer/snap_data/model/squeezenet.caffemodel "
                "input=3:224:224:1 inputtype=float32 inputlayout=NHWC inputname=data "
                "output=1:1:1000:1 outputtype=float32 outputlayout=NCHW outputname=prob "
                "custom=ModelFWType:CAFFE,ExecutionDataType:FLOAT32,ComputingUnit:CPU ! "
            "tensor_sink name=sinkx";

  LOGI ("Start to run NNStreamer pipeline with SNAP.");

  /* construct a pipeline */
  status = ml_pipeline_construct (description, NULL, NULL, &pipe);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to construct the pipeline.");
    goto done;
  }

  /* register sink callback */
  status = ml_pipeline_sink_register (pipe, "sinkx", sample_sink_cb, NULL, &sink);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to construct the pipeline.");
    goto done;
  }

  /* start the pipeline */
  status = ml_pipeline_start (pipe);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to start the pipeline.");
    goto done;
  }

  /* sleep to run pipeline for a while */
  sleep (10);

done:
  /* destroy the pipeline */
  if (ml_pipeline_destroy (pipe) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy pipeline handle.");
  }
}

/**
 * @brief Native method to run sample pipeline with NNStreamer.
 */
jboolean
Java_org_nnsuite_nnstreamer_sample_MainActivity_nativeRunSample (JNIEnv * env, jobject thiz)
{
  LOGI ("Start to run NNStreamer C-API sample.");

  /**
   * Initialize NNStreamer.
   * You SHOULD initialize NNStreamer first.
   */
  if (!nnstreamer_native_initialize ()) {
    LOGE ("Failed to initialize NNStreamer.");
    return JNI_FALSE;
  }

  run_simple_pipeline ();
  run_snap_example ();

  return JNI_TRUE;
}
