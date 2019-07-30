/**
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * @file  main.c
 * @date  9 May 2019
 * @brief Application to test nnstreamer
 * @see   http://github.com/nnsuite/nnstreamer
 * @author  Parichay Kapoor <pk.kapoor@samsung.com>
 * @bug   No known bugs except for NYI items
 *
 */

#include <app.h>
#include <dlog.h>
#include <dlfcn.h>
#include <Elementary.h>
#include <tizen_error.h>
#include <nnstreamer/nnstreamer.h>

#include "main.h"

/**
 * @brief Macro for buffer sizes
 */
#define FILE_BUFFER_SIZE 256
#define FILENAME_BUFFER_SIZE 1024

/**
 * @brief function declaration for verifying so file
 */
typedef void *(*_nns_custom_init_func) (const void *);
struct NNStreamer_custom_class
{
  _nns_custom_init_func initfunc;
};

/**
 * @brief Compare contents of two binary files.
 */
int
compare_output_files (const char *file1, const char *file2)
{
  FILE *fp1, *fp2;
  char tmp1[FILE_BUFFER_SIZE], tmp2[FILE_BUFFER_SIZE];
  size_t n1, n2;

  fp1 = fopen (file1, "rb");
  if (fp1 == NULL) {
    return -1;
  }
  fp2 = fopen (file2, "rb");
  if (fp2 == NULL) {
    fclose (fp1);
    return -1;
  }

  do {
    n1 = fread (tmp1, FILE_BUFFER_SIZE, 1, fp1);
    n2 = fread (tmp2, FILE_BUFFER_SIZE, 1, fp2);

    if (n1 != n2 || memcmp (tmp1, tmp2, n1)) {
      fclose (fp1);
      fclose (fp2);
      return -1;
    }

  } while (n1);

  fclose (fp1);
  fclose (fp2);
  return ML_ERROR_NONE;
}

/**
 * @brief verify that the so file can be loaded properly
 */
int
verify_so_file (const char *filename)
{
  void *dlhandle;
  struct NNStreamer_custom_class *nnst_class;
  char *dlsym_error;

  dlhandle = dlopen (filename, RTLD_NOW);
  if (dlhandle == NULL) {
    dlog_print (DLOG_ERROR, "NNStreamerApp", "Unable to open so file.");
    return -1;
  }

  dlerror ();
  nnst_class =
      *((struct NNStreamer_custom_class **) dlsym (dlhandle,
          "NNStreamer_custom"));
  dlsym_error = dlerror ();
  if (dlsym_error) {
    dlog_print (DLOG_ERROR, "NNStreamerApp",
        "Unable to read symbol from so file with error %s", dlsym_error);
    dlclose (dlhandle);
    return -1;
  }

  dlclose (dlhandle);
  return ML_ERROR_NONE;
}

/**
 * @brief Main function to test run SO file of the application.
 */
int
main (int argc, char *argv[])
{
  char direct_file[FILENAME_BUFFER_SIZE];
  char passthrough_file[FILENAME_BUFFER_SIZE];
  char custom_filter_file[FILENAME_BUFFER_SIZE];
  char pipeline[FILENAME_BUFFER_SIZE];
  void *handle;
  int status;
  ml_pipeline_state_e state;

  snprintf (custom_filter_file, FILENAME_BUFFER_SIZE,
      "%s/libnnstreamer_customfilter_passthrough.so", app_get_resource_path ());
  snprintf (direct_file, FILENAME_BUFFER_SIZE, "%s/testcase01.direct.log",
      app_get_shared_data_path ());
  snprintf (passthrough_file, FILENAME_BUFFER_SIZE,
      "%s/testcase01.passthrough.log", app_get_shared_data_path ());
  snprintf (pipeline, FILENAME_BUFFER_SIZE,
      "videotestsrc num-buffers=1 ! videoconvert ! video/x-raw,format=RGB,width=280,height=40,framerate=0/1 ! "
      "tensor_converter ! tee name=t "
      "t. ! queue ! tensor_filter framework=custom model=%s ! multifilesink location=%s sync=true "
      "t. ! queue ! multifilesink location=\"%s\" sync=true",
      custom_filter_file, passthrough_file, direct_file);

  status = verify_so_file (custom_filter_file);
  if (status != TIZEN_ERROR_NONE) {
    return -1;
  }

  status = ml_pipeline_construct (pipeline, NULL, NULL, &handle);
  if (status != ML_ERROR_NONE) {
    return -1;
  }

  status = ml_pipeline_start (handle);
  if (status != ML_ERROR_NONE) {
    return -1;
  }

  sleep (1);
  status = ml_pipeline_get_state (handle, &state);
  if (status != ML_ERROR_NONE || state != ML_PIPELINE_STATE_PLAYING) {
    return -1;
  }
  status = ml_pipeline_stop (handle);
  if (status != ML_ERROR_NONE) {
    return -1;
  }

  sleep (1);
  status = ml_pipeline_get_state (handle, &state);
  if (status != ML_ERROR_NONE || state != ML_PIPELINE_STATE_PAUSED) {
    return -1;
  }

  status = ml_pipeline_destroy (handle);
  if (status != ML_ERROR_NONE) {
    return -1;
  }

  status = compare_output_files (direct_file, passthrough_file);
  if (status != ML_ERROR_NONE) {
    return -1;
  }

  dlog_print (DLOG_INFO, "NNStreamerApp", "Successfully ran the pipeline");
  return 0;
}
