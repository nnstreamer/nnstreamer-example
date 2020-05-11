/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * NNStreamer example for Tizen Platform with Vivante and Tensorflow Lite.
 * Copyright (C) 2020 HyoungJoo Ahn <hello.ahn@samsung.com>
 */
/**
 * @file main.c
 * @date 11 May 2020
 * @brief TIZEN Native Example App on MESON64 board with Vivante and TensorflowLite
 * @see  https://github.com/nnstreamer/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

#include <glib.h>
#include <stdint.h>
#include <stdio.h>
#include <nnstreamer.h>

#define INCEPTION_MODEL_WIDTH 299
#define INCEPTION_MODEL_HEIGHT  299
#define YOLO_MODEL_WIDTH 416
#define YOLO_MODEL_HEIGHT 416
#define CH  3
#define LABEL_SIZE  5

gchar * vnn_inception_nb;
gchar * vnn_inception_so;
gchar * vnn_yolo_nb;
gchar * vnn_yolo_so;
gchar * tflite_inception_model;

ml_pipeline_h handle;
ml_pipeline_sink_h vnn_inception_sink_handle;
ml_pipeline_sink_h vnn_yolo_sink_handle;
ml_pipeline_sink_h tflite_inception_sink_handle;

gint64 app_start_time;
gint64 app_finish_time;

gint64 pipeline_start_time;
gint64 pipeline_stop_time;

guint vnn_inception_cnt = 0;
guint vnn_yolo_cnt = 0;
guint tflite_inception_cnt = 0;

/**
* @brief print the profile results
*/
void print_profile (){
  gfloat running_time = (pipeline_stop_time - pipeline_start_time) / 1000000.0;
  printf ("\n");
  printf ("===============================================\n");
  printf ("                    SUMMARY\n");
  printf ("===============================================\n");
  printf ("  APP Running Time               : %.2f sec\n", (app_finish_time-app_start_time) / 1000000.0);
  printf ("  pipeline Running Time          : %.2f sec\n", running_time);
  printf ("\n");
  printf ("  VNN_INCEPTION_TOTAL Called     : %d\n", vnn_inception_cnt);
  printf ("  VNN_YOLO_TOTAL Called          : %d\n", vnn_yolo_cnt);
  printf ("  TFLITE_INCEPTION_TOTAL Called  : %d\n", tflite_inception_cnt);
  printf ("\n");
  printf ("  VNN_INCEPTION_TOTAL Average    : %.2f/sec\n", vnn_inception_cnt / running_time);
  printf ("  VNN_YOLO_TOTAL Average         : %.2f/sec\n", vnn_yolo_cnt / running_time);
  printf ("  TFLITE_INCEPTION_TOTAL Average : %.2f/sec\n", tflite_inception_cnt / running_time);
  printf ("===============================================\n");
}

/**
 * @brief get the error name rather than code number
 */
char* getErrorName (int status){
  switch (status){
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
get_vnn_inception_result_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  vnn_inception_cnt++;
}

/**
 * @brief get label data when tensor_sink receive the output from the model
 */
void
get_vnn_yolo_result_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  vnn_yolo_cnt++;
}

/**
 * @brief get label data when tensor_sink receive the output from the model
 */
void
get_tflite_inception_result_from_tensor_sink (const ml_tensors_data_h data,
    const ml_tensors_info_h info, void *user_data)
{
  tflite_inception_cnt++;
}

/**
 * @brief main function
 */
int main (int argc, char *argv[]){
  int status;
  gboolean enable_vnn_inception = false;
  gboolean enable_vnn_yolo = false;
  gboolean enable_tflite_inception = false;

  app_start_time = g_get_real_time ();

  if (g_strcmp0 (argv[1], "1") == 0)
    enable_vnn_inception = true;
  if (g_strcmp0 (argv[2], "1") == 0)
    enable_vnn_yolo = true;
  if (g_strcmp0 (argv[3], "1") == 0)
    enable_tflite_inception = true;
  if (!enable_vnn_inception &&
      !enable_vnn_yolo &&
      !enable_tflite_inception){
    enable_vnn_inception = true;
    enable_vnn_yolo = true;
    enable_tflite_inception = true;
  }

  GString *pipeline = g_string_new ("v4l2src ! tee name=t");

  if (enable_vnn_inception){
    vnn_inception_nb = g_strdup_printf ("/usr/share/dann/inception-v3.nb");
    vnn_inception_so = g_strdup_printf ("/usr/share/vivante/inceptionv3/libinceptionv3.so");
    g_string_append_printf (pipeline, 
      " t. ! queue ! videoscale ! "
      "video/x-raw,format=RGB,width=%d,height=%d ! "
      "tensor_converter ! "
      "queue max-size-buffers=2 leaky=2 ! "
      "tensor_filter framework=vivante model=\"%s,%s\" silent=false ! "
      "tensor_sink name=vnn_inception_sink",
      INCEPTION_MODEL_WIDTH, INCEPTION_MODEL_HEIGHT, vnn_inception_nb, vnn_inception_so
      );
  }

  if (enable_vnn_yolo){
    vnn_yolo_nb = g_strdup_printf ("/usr/share/dann/yolo-v3.nb");
    vnn_yolo_so = g_strdup_printf ("/usr/share/vivante/yolov3/libyolov3.so");
    g_string_append_printf (pipeline, 
      " t. ! queue ! videoscale ! "
      "video/x-raw,format=RGB,width=%d,height=%d ! "
      "tensor_converter ! "
      "tensor_transform mode=transpose option=1:2:0:3 ! "
      "tensor_transform mode=arithmetic option=div:2 ! "
      "tensor_transform mode=typecast option=int8 ! "
      "queue max-size-buffers=2 leaky=2 ! "
      "tensor_filter framework=vivante model=\"%s,%s\" silent=false ! "
      "tensor_sink name=vnn_yolo_sink",
      YOLO_MODEL_WIDTH, YOLO_MODEL_HEIGHT, vnn_yolo_nb, vnn_yolo_so
      );
  }

  if (enable_tflite_inception){
    tflite_inception_model = g_strdup_printf ("/usr/share/vivante/inception_v3_quant.tflite");
    g_string_append_printf (pipeline, 
      " t. ! queue ! videoscale ! "
      "video/x-raw,format=RGB,width=%d,height=%d ! "
      "tensor_converter ! "
      "queue max-size-buffers=2 leaky=2 ! "
      "tensor_filter framework=tensorflow-lite model=\"%s\" silent=false ! "
      "tensor_sink name=tflite_inception_sink",
      INCEPTION_MODEL_WIDTH, INCEPTION_MODEL_HEIGHT, tflite_inception_model
      );
  }

  printf ("[%d] pipeline: %s\n\n", __LINE__, pipeline->str);
  status = ml_pipeline_construct (pipeline->str, NULL, NULL, &handle);
  if (status != ML_ERROR_NONE){
    printf ("[%d] ERROR!!!: %s\n", __LINE__, getErrorName (status));
    goto error;
  }

  if (enable_vnn_inception){
    /* vnn inception v3 */
    status = ml_pipeline_sink_register (handle, "vnn_inception_sink",
      get_vnn_inception_result_from_tensor_sink, NULL,
      &vnn_inception_sink_handle
    );
    if (status != ML_ERROR_NONE){
      printf ("[%d] ERROR!!!: %s\n", __LINE__, getErrorName (status));
      goto error;
    }
  }
  if (enable_vnn_yolo){
    /* vnn yolo v3 */
    status = ml_pipeline_sink_register (handle, "vnn_yolo_sink",
      get_vnn_yolo_result_from_tensor_sink, NULL,
      &vnn_yolo_sink_handle
    );
    if (status != ML_ERROR_NONE){
      printf ("[%d] ERROR!!!: %s\n", __LINE__, getErrorName (status));
      goto error;
    }
  }
  if (enable_tflite_inception){
    /* tflite inception v3 */
    status = ml_pipeline_sink_register (handle, "tflite_inception_sink",
      get_tflite_inception_result_from_tensor_sink, NULL,
      &tflite_inception_sink_handle
    );
    if (status != ML_ERROR_NONE){
      printf ("[%d] ERROR!!!: %s\n", __LINE__, getErrorName (status));
      goto error;
    }
  }

  status = ml_pipeline_start (handle);
  if (status != ML_ERROR_NONE){
    printf ("[%d] ERROR!!!: %s\n", __LINE__, getErrorName (status));
    goto error;
  }
  pipeline_start_time = g_get_real_time ();

  /* run pipeline for 10 secs */
  g_usleep (10 * 1000000);

error:
  ml_pipeline_stop (handle);
  pipeline_stop_time = g_get_real_time ();

  ml_pipeline_sink_unregister (vnn_inception_sink_handle);
  ml_pipeline_sink_unregister (vnn_yolo_sink_handle);
  ml_pipeline_sink_unregister (tflite_inception_sink_handle);
  ml_pipeline_destroy (handle);

  g_free (vnn_inception_nb);
  g_free (vnn_inception_so);
  g_free (vnn_yolo_nb);
  g_free (vnn_yolo_so);
  g_free (tflite_inception_model);

  g_string_free (pipeline, FALSE);

  app_finish_time = g_get_real_time ();
  print_profile ();

  return 0;
}
