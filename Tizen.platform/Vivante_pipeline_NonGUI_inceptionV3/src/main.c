/**
 * @file main.c
 * @date 19 Feb 2020
 * @brief TIZEN Native Example App of Vivante Inception V3 with pipeline capi
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <nnstreamer.h>

#define MODEL_WIDTH 299
#define MODEL_HEIGHT  299
#define CH  3
#define LABEL_SIZE  5

gchar * pipeline;
gchar * res_path;
gchar * img_path;
gchar * model1_path;
gchar * model2_path;

ml_pipeline_h handle;
ml_pipeline_sink_h sinkhandle;

/**
 * @brief get the error name rather than code number
 */
char* getErrorName(int status){
  switch(status){
    case TIZEN_ERROR_NONE:
      return "TIZEN_ERROR_NONE";
    case TIZEN_ERROR_INVALID_PARAMETER:
      return "TIZEN_ERROR_INVALID_PARAMETER";
    case TIZEN_ERROR_STREAMS_PIPE:
      return "TIZEN_ERROR_STREAMS_PIPE";
    case TIZEN_ERROR_TRY_AGAIN:
      return "TIZEN_ERROR_TRY_AGAIN";
    case TIZEN_ERROR_UNKNOWN:
      return "TIZEN_ERROR_UNKNOWN";
    case TIZEN_ERROR_TIMED_OUT:
      return "TIZEN_ERROR_TIMED_OUT";
    case TIZEN_ERROR_NOT_SUPPORTED:
      return "TIZEN_ERROR_NOT_SUPPORTED";
    case TIZEN_ERROR_PERMISSION_DENIED:
      return "TIZEN_ERROR_PERMISSION_DENIED";
    case TIZEN_ERROR_OUT_OF_MEMORY:
      return "TIZEN_ERROR_OUT_OF_MEMORY";
    default:
      return "UNKNOWN";
  }
}

/**
 * @brief get label data when tensor_sink receive the output from the model
 */
void
get_label_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  size_t data_size;
  uint16_t *output;
  int label_num = -1;
  uint16_t max = 0;

  int status = ml_tensors_data_get_tensor_data (data, 0, (void **) &output, &data_size);
  if (status != ML_ERROR_NONE){
    printf("[%d]ERROR %s\n", __LINE__, getErrorName(status));
    return;
  } else {
    printf("Data extracted well\n");
  }
  printf("datasize %llu\n", data_size);
  for (int i = 0; i < LABEL_SIZE; i++) {
    if (output[i] > 0 && output[i] > max) {
      max = output[i];
      label_num = i;
      printf("max: %d, labelnum: %d\n", max, label_num);
    }
  }

  printf(">>> RESULT LABEL NUMBER: %d\n", label_num);
  ml_pipeline_stop(handle);

  ml_pipeline_sink_unregister(sinkhandle);
  ml_pipeline_destroy(handle);

  g_free(model1_path);
  g_free(model2_path);
  g_free(img_path);
  g_free(res_path);
  g_free(pipeline);
}

/**
 * @brief main function
 */
int main(int argc, char *argv[]){
  int status;
  img_path = g_strdup_printf("%s", argv[1]);
  model1_path = g_strdup_printf("/usr/share/dann/inception-v3.nb");
  model2_path = g_strdup_printf("/usr/lib/nnstreamer/filters/libinceptionv3.so");

  printf("IMAGE path: %s\n", img_path);
  printf("MODEL1 path: %s\n", model1_path);
  printf("MODEL2 path: %s\n", model2_path);

  if(!g_file_test (model1_path, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", model1_path);
    return 0;
  }

  if(!g_file_test (model2_path, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", model2_path);
    return 0;
  }

  if(!g_file_test (img_path, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", img_path);
    return 0;
  }

  pipeline =
      g_strdup_printf(
          "filesrc location=\"%s\" blocksize=-1 ! jpegdec ! "
          "videoconvert ! "
          "video/x-raw,format=RGB,width=%d,height=%d ! "
          "tensor_converter ! "
          "tensor_filter framework=vivante model=\"%s,%s\" ! "
          "tensor_sink name=sinkx", 
          img_path,
          MODEL_WIDTH,
          MODEL_HEIGHT,
          model1_path,
          model2_path
          );

  printf("[%d] pipeline: %s\n", __LINE__, pipeline);
  status = ml_pipeline_construct(pipeline, NULL, NULL, &handle);
  if (status != ML_ERROR_NONE){
    printf("[%d] ERROR!!!: %s\n", __LINE__, getErrorName(status));
    return 0;
  }

  status = ml_pipeline_sink_register(handle, "sinkx", get_label_from_tensor_sink, NULL,
      &sinkhandle);
  if (status != ML_ERROR_NONE){
    printf("[%d] ERROR!!!: %s\n", __LINE__, getErrorName(status));
    return 0;
  }

  status = ml_pipeline_start(handle);
  if (status != ML_ERROR_NONE){
    printf("[%d] ERROR!!!: %s\n", __LINE__, getErrorName(status));
    return 0;
  }

  g_usleep(100000);

  return 0;
}
