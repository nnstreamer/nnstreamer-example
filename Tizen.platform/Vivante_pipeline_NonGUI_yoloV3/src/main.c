/**
 * @file main.c
 * @date 04 Mar 2020
 * @brief TIZEN Native Example App of Vivante Yolo V3 with pipeline capi
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

#include <glib.h>
#include <stdint.h>  /* integer types such as int8_t, init16_t, ... */
#include <stdio.h>
#include <nnstreamer.h>

#define MODEL_NB 1
#define MODEL_SO 2
#define JPG_PATH 3
#define BIN_PATH 4

#define MODEL_WIDTH 416
#define MODEL_HEIGHT 416

#define BUFF_SIZE 43095 + 172380 + 689520

gchar * pipeline;
gchar * img_path;
gchar * model_nb;
gchar * model_so;
gchar * bin_path;
char   buff[BUFF_SIZE];

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
 * In/Out tensors
 * D [print_tensor:136]in(0) : id[   0] vtl[0] const[0] shape[ 416, 416, 3, 1   ] fmt[i8 ] qnt[DFP fl=  7]
 * D [print_tensor:136]out(0): id[   1] vtl[0] const[0] shape[ 13, 13, 255, 1   ] fmt[i8 ] qnt[DFP fl=  2]
 * D [print_tensor:136]out(1): id[   2] vtl[0] const[0] shape[ 26, 26, 255, 1   ] fmt[i8 ] qnt[DFP fl=  2]
 * D [print_tensor:136]out(2): id[   3] vtl[0] const[0] shape[ 52, 52, 255, 1   ] fmt[i8 ] qnt[DFP fl=  2]
 */
void
get_label_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  size_t out_size1, out_size2, out_size3;
  uint8_t *output1; // model output is uint8 rather than int8
  uint8_t *output2;
  uint8_t *output3;
  int i;

  int status = ml_tensors_data_get_tensor_data (data, 0, (void **) &output1, &out_size1);
  if (status != ML_ERROR_NONE){
    printf("[%d]ERROR %s\n", __LINE__, getErrorName(status));
    return;
  }
  printf("out_size1 %u\n", out_size1);

  status = ml_tensors_data_get_tensor_data (data, 1, (void **) &output2, &out_size2);
  if (status != ML_ERROR_NONE){
    printf("[%d]ERROR %s\n", __LINE__, getErrorName(status));
    return;
  }
  printf("out_size2 %u\n", out_size2);

  status = ml_tensors_data_get_tensor_data (data, 2, (void **) &output3, &out_size3);
  if (status != ML_ERROR_NONE){
    printf("[%d]ERROR %s\n", __LINE__, getErrorName(status));
    return;
  }
  printf("out_size3 %u\n", out_size3);

  for(i = 0; i < out_size1; i++){
    if(output1[i] != buff[i]){
      printf("DIFFERENT RESULT!!! %d != %d\n", output1[i], buff[i]);
      return;
    }
  }
  printf("Result comparision for 13x13x255 layer has been done successfully.\n");
  for(i = 0; i < out_size2; i++){
    if(output2[i] != buff[i + out_size1]){
      printf("DIFFERENT RESULT!!! %d != %d\n", output2[i], buff[i + out_size1]);
      return;
    }
  }
  printf("Result comparision for 26x26x255 layer has been done successfully.\n");
  for(i = 0; i < out_size3; i++){
    if(output3[i] != buff[i + out_size1 + out_size2]){
      printf("DIFFERENT RESULT!!! %d != %d\n", output3[i], buff[i + out_size1 + out_size2]);
      return;
    }
  }
  printf("Result comparision for 52x52x255 layer has been done successfully.\n");

  ml_pipeline_stop(handle);

  ml_pipeline_sink_unregister(sinkhandle);
  ml_pipeline_destroy(handle);

  g_free(model_nb);
  g_free(model_so);
  g_free(img_path);
  g_free(bin_path);
  g_free(pipeline);
}

/**
 * @brief main function
 */
int main(int argc, char *argv[]){
  int status;
  model_nb = g_strdup_printf("%s", argv[MODEL_NB]);
  model_so = g_strdup_printf("%s", argv[MODEL_SO]);
  img_path = g_strdup_printf("%s", argv[JPG_PATH]);
  bin_path = g_strdup_printf("%s", argv[BIN_PATH]);

  printf("MODEL_NB path: %s\n", model_nb);
  printf("MODEL_SO path: %s\n", model_so);
  printf("IMAGE path: %s\n", img_path);
  printf("BIN path: %s\n", bin_path);

  if(!g_file_test (model_nb, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", model_nb);
    return 0;
  }

  if(!g_file_test (model_so, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", model_so);
    return 0;
  }

  if(!g_file_test (img_path, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", img_path);
    return 0;
  }

  if(!g_file_test (bin_path, G_FILE_TEST_EXISTS)){
    printf("[%s] IS NOT EXISTED!!\n", bin_path);
    return 0;
  }

  /* read result from `gst-launch-1.0` */
  int fd;
  if (0 < (fd = open(bin_path, 'r')))
  {
    read(fd, buff, BUFF_SIZE);
    puts(buff);
    close(fd);
  }
  else{
    printf("%s is not readable!!!\n", bin_path);
    return 0;
  }

  pipeline =
      g_strdup_printf(
          "filesrc location=\"%s\" ! jpegdec ! "
          "videoconvert ! "
          "video/x-raw,format=BGR,width=%d,height=%d ! "
          "tensor_converter ! "
          "tensor_transform mode=transpose option=1:2:0:3 ! "
          "tensor_transform mode=arithmetic option=div:2 ! "
          "tensor_transform mode=typecast option=int8 ! "
          "tensor_filter framework=vivante model=\"%s,%s\" ! "
          "tensor_sink name=sinkx", 
          img_path,
          MODEL_WIDTH,
          MODEL_HEIGHT,
          model_nb,
          model_so
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

  int sleep = 2000000;
  printf("g_usleep [%d]: wait for the model result\n", sleep);
  g_usleep(sleep);

  return 0;
}
