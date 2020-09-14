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
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <android/log.h>

#include "nnstreamer.h"
#include "nnstreamer-single.h"

#define TAG_NAME "NNStreamer-Sample"
#define LOGD(...) __android_log_print (ANDROID_LOG_DEBUG, TAG_NAME, __VA_ARGS__)
#define LOGI(...) __android_log_print (ANDROID_LOG_INFO, TAG_NAME, __VA_ARGS__)
#define LOGE(...) __android_log_print (ANDROID_LOG_ERROR, TAG_NAME, __VA_ARGS__)

/**
 * @brief Initialize NNStreamer defined in NNStreamer module.
 */
extern jboolean nnstreamer_native_initialize (JNIEnv * env, jobject context);

/**
 * @brief Internal function to check the framework is available.
 */
static int
check_nnfw (ml_nnfw_type_e type)
{
  int status;
  bool available = false;

  status = ml_check_nnfw_availability (type, ML_NNFW_HW_ANY, &available);

  return (status == ML_ERROR_NONE && available);
}

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
 * @brief SNAP pipeline example.
 */
static void
run_example_snap_pipeline (bool is_tf)
{
  ml_pipeline_h pipe = NULL;
  ml_pipeline_sink_h sink = NULL;
  int status = ML_ERROR_NONE;

  /**
   * Pipeline with SNAP
   *
   * Set custom properties in tensor-filter.
   *
   * GPU: "ComputingUnit:GPU,GpuCacheSource:/sdcard/nnstreamer/"
   * Denote GPU cache path. (GpuCacheSource:cache-path)
   * It takes a time with GPU option in the first run.
   *
   * CPU: "ComputingUnit:CPU"
   * Set CPU thread count. (default 4. e.g., CpuThreadCount:5 to set the count 5.)
   */
  const char desc_caffe[] = "videotestsrc ! videoconvert ! videoscale ! "
            "video/x-raw,format=RGB,width=224,height=224,framerate=(fraction)30/1 ! "
            "tensor_converter ! tensor_transform mode=typecast option=float32 ! "
            "tensor_filter framework=snap "
                "model=/sdcard/nnstreamer/snap_data/prototxt/squeezenet.prototxt,"
                    "/sdcard/nnstreamer/snap_data/model/squeezenet.caffemodel "
                "input=3:224:224:1 inputtype=float32 inputlayout=NHWC inputname=data "
                "output=1:1:1000:1 outputtype=float32 outputlayout=NCHW outputname=prob "
                "custom=ModelFWType:CAFFE,ExecutionDataType:FLOAT32,ComputingUnit:CPU ! "
            "tensor_sink name=sinkx";

  const char desc_tf[] = "videotestsrc ! videoconvert ! videoscale ! "
            "video/x-raw,format=RGB,width=224,height=224,framerate=(fraction)30/1 ! "
            "tensor_converter ! tensor_transform mode=typecast option=float32 ! "
            "tensor_filter framework=snap "
                "model=/sdcard/nnstreamer/snap_data/model/yolo_new.pb "
                "input=3:224:224:1 inputtype=float32 inputlayout=NHWC inputname=input "
                "output=1001:1 outputtype=float32 outputlayout=NHWC outputname=MobilenetV1/Predictions/Reshape_1 "
                "custom=ModelFWType:TENSORFLOW,ExecutionDataType:FLOAT32,ComputingUnit:CPU ! "
            "tensor_sink name=sinkx";

  if (!check_nnfw (ML_NNFW_TYPE_SNAP)) {
    LOGI ("SNAP is not available, skip this example.");
    return;
  }

  LOGI ("Start to run NNStreamer pipeline with SNAP.");

  /* construct a pipeline */
  status = ml_pipeline_construct ((is_tf) ? desc_tf : desc_caffe, NULL, NULL, &pipe);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to construct the pipeline.");
    goto done;
  }

  /* register sink callback */
  status = ml_pipeline_sink_register (pipe, "sinkx", sample_sink_cb, NULL, &sink);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get the sink handle.");
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
 * @brief NNFW pipeline example.
 */
static void
run_example_nnfw_pipeline (void)
{
  ml_pipeline_h pipe = NULL;
  ml_pipeline_sink_h sink = NULL;
  ml_pipeline_src_h src = NULL;
  ml_tensors_info_h in_info = NULL;
  ml_tensors_data_h in_data = NULL;
  int status = ML_ERROR_NONE;
  float *data = NULL;
  size_t data_size;

  const char description[] = "appsrc name=srcx ! "
             "other/tensor,dimension=(string)1:1:1:1,type=(string)float32,framerate=(fraction)0/1 ! "
             "tensor_filter framework=nnfw model=/sdcard/nnstreamer/tflite_model_add/add.tflite ! "
             "tensor_sink name=sinkx";

  if (!check_nnfw (ML_NNFW_TYPE_NNFW)) {
    LOGI ("NNFW is not available, skip this example.");
    return;
  }

  LOGI ("Start to run NNStreamer pipeline with NNFW.");

  /* construct a pipeline */
  status = ml_pipeline_construct (description, NULL, NULL, &pipe);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to construct the pipeline.");
    goto done;
  }

  /* register sink callback */
  status = ml_pipeline_sink_register (pipe, "sinkx", sample_sink_cb, NULL, &sink);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get the sink handle.");
    goto done;
  }

  /* get src handle */
  status = ml_pipeline_src_get_handle (pipe, "srcx", &src);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get the src handle.");
    goto done;
  }

  status = ml_pipeline_src_get_tensors_info (src, &in_info);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get the input info.");
    goto done;
  }

  /* start the pipeline */
  status = ml_pipeline_start (pipe);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to start the pipeline.");
    goto done;
  }

  /* input data */
  status = ml_tensors_data_create (in_info, &in_data);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to create input data.");
    goto done;
  }

  status = ml_tensors_data_get_tensor_data (in_data, 0, (void **) &data, &data_size);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get input data.");
    goto done;
  }

  *data = 10.2f;
  status = ml_tensors_data_set_tensor_data (in_data, 0, data, sizeof (float));
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to set input data.");
    goto done;
  }

  /* auto-free, do not destroy data handle. */
  status = ml_pipeline_src_input_data (src, in_data, ML_PIPELINE_BUF_POLICY_AUTO_FREE);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to push input data.");
    goto done;
  }

  /* sleep to run pipeline for a while */
  sleep (1);

done:
  /* destroy the pipeline */
  if (ml_pipeline_destroy (pipe) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy pipeline handle.");
  }

  if (ml_tensors_info_destroy (in_info) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy input info handle.");
  }
}

/**
 * @brief NNFW single-shot example.
 */
static void
run_example_nnfw_single (void)
{
  ml_single_h single = NULL;
  ml_tensors_info_h in_info = NULL;
  ml_tensors_data_h in_data = NULL;
  ml_tensors_data_h out_data = NULL;
  int status = ML_ERROR_NONE;
  float *data = NULL;
  size_t data_size;

  const char nnfw_model[] = "/sdcard/nnstreamer/tflite_model_add/add.tflite";

  if (!check_nnfw (ML_NNFW_TYPE_NNFW)) {
    LOGI ("NNFW is not available, skip this example.");
    return;
  }

  LOGI ("Start to run NNStreamer single-shot with NNFW.");

  status = ml_single_open (&single, nnfw_model, NULL, NULL, ML_NNFW_TYPE_NNFW, ML_NNFW_HW_CPU);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to create handle for single.");
    goto done;
  }

  status = ml_single_get_input_info (single, &in_info);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get input info.");
    goto done;
  }

  /* input data */
  status = ml_tensors_data_create (in_info, &in_data);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to create input data.");
    goto done;
  }

  status = ml_tensors_data_get_tensor_data (in_data, 0, (void **) &data, &data_size);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get input data.");
    goto done;
  }

  *data = 10.2f;
  status = ml_tensors_data_set_tensor_data (in_data, 0, data, sizeof (float));
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to set input data.");
    goto done;
  }

  /* invoke */
  status = ml_single_invoke (single, in_data, &out_data);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to invoke.");
    goto done;
  }

  status = ml_tensors_data_get_tensor_data (out_data, 0, (void **) &data, &data_size);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get output data.");
    goto done;
  }

  if (*data != 12.2f) {
    LOGE ("Invalid result : %f", *data);
  }

done:
  if (ml_single_close (single) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy single handle.");
  }

  if (ml_tensors_info_destroy (in_info) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy input info handle.");
  }

  if (ml_tensors_data_destroy (in_data) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy input data handle.");
  }

  if (ml_tensors_data_destroy (out_data) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy output data handle.");
  }
}

/**
 * @brief SNPE pipeline example.
 */
static void
run_example_snpe_pipeline (void)
{
  ml_pipeline_h pipe = NULL;
  ml_pipeline_sink_h sink = NULL;
  ml_pipeline_src_h src = NULL;
  ml_tensors_info_h in_info = NULL;
  ml_tensors_data_h in_data = NULL;
  int status = ML_ERROR_NONE;
  float *dummy_input = NULL;
  size_t input_data_size;

  const char description[] = "appsrc name=srcx ! "
      "other/tensor,dimension=(string)3:299:299:1,type=(string)float32,framerate=(fraction)0/1 ! "
      "tensor_filter framework=snpe model=/sdcard/nnstreamer/snpe_data/inception_v3_quantized.dlc custom=Runtime:DSP ! "
      "tensor_sink name=sinkx";

  if (!check_nnfw (ML_NNFW_TYPE_SNPE)) {
    LOGI ("SNPE is not available, skip this example.");
    return;
  }

  LOGI ("Start to run NNStreamer pipeline with SNPE.");

  /* construct a pipeline */
  status = ml_pipeline_construct (description, NULL, NULL, &pipe);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to construct the pipeline.");
    goto done;
  }

  /* register sink callback */
  status = ml_pipeline_sink_register (pipe, "sinkx", sample_sink_cb, NULL, &sink);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get the sink handle.");
    goto done;
  }

  /* get src handle */
  status = ml_pipeline_src_get_handle (pipe, "srcx", &src);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get the src handle.");
    goto done;
  }

  status = ml_pipeline_src_get_tensors_info (src, &in_info);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get the input info.");
    goto done;
  }

  /* start the pipeline */
  status = ml_pipeline_start (pipe);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to start the pipeline.");
    goto done;
  }

  /* input data */
  status = ml_tensors_data_create (in_info, &in_data);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to create input data.");
    goto done;
  }

  status = ml_tensors_data_get_tensor_data (in_data, 0, (void **) &dummy_input, &input_data_size);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get input data.");
    goto done;
  }

  dummy_input = (float *) malloc (3 * 299 * 299 * sizeof (float));
  status = ml_tensors_data_set_tensor_data (in_data, 0, dummy_input, input_data_size);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to set input data.");
    goto done;
  }

  /* auto-free, do not destroy data handle. */
  status = ml_pipeline_src_input_data (src, in_data, ML_PIPELINE_BUF_POLICY_AUTO_FREE);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to push input data.");
    goto done;
  }

  /* sleep to run pipeline for a while */
  sleep (1);

done:
  /* destroy the pipeline */
  if (ml_pipeline_destroy (pipe) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy pipeline handle.");
  }

  if (ml_tensors_info_destroy (in_info) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy input info handle.");
  }

  free (dummy_input);
}

/**
 * @brief SNPE single-shot example.
 */
static void
run_example_snpe_single (void)
{
  ml_single_h single = NULL;
  ml_tensors_info_h in_info = NULL;
  ml_tensors_data_h in_data = NULL;
  ml_tensors_data_h out_data = NULL;
  int status = ML_ERROR_NONE;
  float *dummy_input = NULL;
  float *dummy_output;
  size_t input_data_size, output_data_size;

  const char snpe_model[] = "/sdcard/nnstreamer/snpe_data/inception_v3_quantized.dlc";

  if (!check_nnfw (ML_NNFW_TYPE_SNPE)) {
    LOGI ("NNFW is not available, skip this example.");
    return;
  }

  LOGI ("Start to run NNStreamer single-shot with SNPE.");

  status = ml_single_open (&single, snpe_model, NULL, NULL, ML_NNFW_TYPE_SNPE, ML_NNFW_HW_ANY);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to create handle for single.");
    goto done;
  }

  status = ml_single_get_input_info (single, &in_info);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get input info.");
    goto done;
  }

  /* input data */
  status = ml_tensors_data_create (in_info, &in_data);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to create input data.");
    goto done;
  }

  status = ml_tensors_data_get_tensor_data (in_data, 0, (void **) &dummy_input, &input_data_size);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get input data.");
    goto done;
  }

  dummy_input = (float *) malloc (3 * 299 * 299 * sizeof (float));
  status = ml_tensors_data_set_tensor_data (in_data, 0, dummy_input, input_data_size);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to set input data.");
    goto done;
  }

  /* invoke */
  status = ml_single_invoke (single, in_data, &out_data);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to invoke.");
    goto done;
  }

  dummy_output = (float *) malloc (1001 * sizeof (float));
  output_data_size = 1001 * sizeof (float);
  status = ml_tensors_data_get_tensor_data (out_data, 0, (void **) &dummy_output, &output_data_size);
  if (status != ML_ERROR_NONE) {
    LOGE ("Failed to get output data.");
    goto done;
  }

done:
  if (ml_single_close (single) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy single handle.");
  }

  if (ml_tensors_info_destroy (in_info) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy input info handle.");
  }

  if (ml_tensors_data_destroy (in_data) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy input data handle.");
  }

  if (ml_tensors_data_destroy (out_data) != ML_ERROR_NONE) {
    LOGE ("Failed to destroy output data handle.");
  }

  free (dummy_input);
  free (dummy_output);
}

/**
 * @brief thread to run the examples.
 */
static pthread_t g_app_thread;

/**
 * @brief Run the examples.
 */
static void *
run_examples (void *user_data)
{
  run_simple_pipeline ();
  run_example_nnfw_pipeline ();
  run_example_nnfw_single ();
  run_example_snap_pipeline (false);
  run_example_snpe_pipeline ();
  run_example_snpe_single ();
  return NULL;
}

/**
 * @brief Native method to run sample pipeline with NNStreamer.
 */
jboolean
Java_org_nnsuite_nnstreamer_sample_MainActivity_nativeRunSample (JNIEnv * env, jobject thiz, jobject context)
{
  LOGI ("Start to run NNStreamer C-API sample.");

  /**
   * Initialize NNStreamer.
   * You SHOULD initialize NNStreamer first.
   * Call nnstreamer_native_initialize() in native, or NNStreamer.initialize() in java.
   */
  if (!nnstreamer_native_initialize (env, context)) {
    LOGE ("Failed to initialize NNStreamer.");
    return JNI_FALSE;
  }

  /* Do not run the examples on UI thread */
  pthread_create (&g_app_thread, NULL, &run_examples, NULL);
  return JNI_TRUE;
}
