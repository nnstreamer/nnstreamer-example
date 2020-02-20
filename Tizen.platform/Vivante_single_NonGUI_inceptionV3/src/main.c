/**
 * @file main.c
 * @date 19 Feb 2020
 * @brief TIZEN Native Example App of Vivante Inception V3 with single capi
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <nnstreamer.h>
#include <nnstreamer-single.h>

#define MODEL_WIDTH 299
#define MODEL_HEIGHT  299
#define CH  3
#define LABEL_SIZE  5

gchar * res_path;
gchar * img_path;
gchar * model_path;
gchar * model1_path;
gchar * model2_path;

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
 * @brief read data source file
 */
 int readInputSource(void **input_buf, size_t data_size)
{
  FILE *fileptr;
  long filelen;
  int ret = -1;

  fileptr = fopen(img_path, "rb");
  fseek(fileptr, 0, SEEK_END);
  filelen = ftell(fileptr);
  rewind(fileptr);

  if(data_size == filelen){
    fread(*input_buf, filelen, 1, fileptr);
    ret = 0;
  }
  fclose(fileptr);

  return ret;
}

/**
 * @brief main function
 */
int main(int argc, char *argv[]){
  ml_single_h single;
  ml_tensors_info_h in_info = NULL;
  ml_tensors_data_h input_data = NULL;
  ml_tensors_data_h output_data = NULL;
  uint16_t *output_buf = NULL;
  void *input_buf = NULL;
  size_t data_size;
  int max = -1;
  int label_num = -1;

  int status;
  img_path = g_strdup_printf("%s", argv[1]);
  model1_path = g_strdup_printf("/usr/share/dann/inception-v3.nb");
  model2_path = g_strdup_printf("/usr/lib/nnstreamer/filters/libinceptionv3.so");
  model_path = g_strdup_printf("%s,%s", model1_path, model2_path);

  printf("IMAGE path: %s\n", img_path);
  printf("MODEL1 path: %s\n", model1_path);
  printf("MODEL2 path: %s\n", model2_path);

  if(!g_file_test (model1_path, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", model1_path);
    goto finish;
  }

  if(!g_file_test (model2_path, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", model2_path);
    goto finish;
  }

  if(!g_file_test (img_path, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", img_path);
    goto finish;
  }

  input_buf = (uint8_t *)malloc(MODEL_WIDTH * MODEL_HEIGHT * CH * sizeof(uint8_t));
  status = readInputSource(&input_buf, MODEL_WIDTH * MODEL_HEIGHT * CH);
  if(status != ML_ERROR_NONE){
    printf("[%d] FILE READ ERROR\n", __LINE__);
    goto finish;
  }

  // status = ml_single_open(&single, model_path, NULL, NULL,
  //     ML_NNFW_TYPE_VIVANTE, ML_NNFW_HW_ANY);
  status = ml_single_open(&single, model_path, NULL, NULL,
      ML_NNFW_TYPE_ANY, ML_NNFW_HW_ANY);

  status = ml_single_get_input_info(single, &in_info);
  if(status != ML_ERROR_NONE){
    printf("[%d] ERROR: %s\n", __LINE__, getErrorName(status));
    goto finish;
  }

  status = ml_tensors_data_create(in_info, &input_data);
  if(status != ML_ERROR_NONE){
    printf("[%d] ERROR: %s\n", __LINE__, getErrorName(status));
    goto finish;
  }

  status = ml_tensors_data_set_tensor_data(input_data, 0, input_buf,
      MODEL_WIDTH * MODEL_HEIGHT * CH);
  if(status != ML_ERROR_NONE){
    printf("[%d] ERROR: %s\n", __LINE__, getErrorName(status));
    goto finish;
  }

  status = ml_single_invoke(single, input_data, &output_data);
  if(status != ML_ERROR_NONE){
    printf("[%d] ERROR: %s\n", __LINE__, getErrorName(status));
    goto finish;
  }

  status = ml_tensors_data_get_tensor_data(output_data, 0, (void **) &output_buf,
      &data_size);
  if(status != ML_ERROR_NONE){
    printf("[%d] ERROR: %s\n", __LINE__, getErrorName(status));
    goto finish;
  }

  for (int i = 0; i < data_size; i++) {
    if (output_buf[i] > 0 && output_buf[i] > max) {
      max = output_buf[i];
      label_num = i;
      printf("max: %d, labelnum: %d\n", max, label_num);
    }
  }
  printf(">>> RESULT LABEL NUMBER: %d\n", label_num);

finish:
  ml_tensors_data_destroy(output_data);
  ml_tensors_data_destroy(input_data);
  ml_tensors_info_destroy(in_info);
  ml_single_close(single);
  g_free(model_path);
  g_free(model1_path);
  g_free(model2_path);
  g_free(img_path);
  g_free(input_buf);

  return 0;
}
