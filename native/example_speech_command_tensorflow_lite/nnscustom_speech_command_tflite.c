/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * NNStreamer custom filter for speech command example
 * Copyright (C) 2018 Jaeyun Jung <jy1210.jung@samsung.com>
 *
 * @file	nnscustom_speech_command_tflite.c
 * @date	29 Jan 2019
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 * @brief	Custom filter to generate multi tensors from audio stream, used for speech command example.
 * @bug		No known bugs
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <nnstreamer/tensor_filter_custom.h>

/**
 * @brief nnstreamer custom filter private data
 */
typedef struct _pt_data
{
  unsigned int num_audio_stream; /**< This counts incoming stream */
} pt_data;

/**
 * @brief nnstreamer custom filter standard vmethod
 * Refer tensor_filter_custom.h
 */
static void *
pt_init (const GstTensorFilterProperties * prop)
{
  pt_data *data = (pt_data *) malloc (sizeof (pt_data));

  assert (data);
  data->num_audio_stream = 0;

  return data;
}

/**
 * @brief nnstreamer custom filter standard vmethod
 * Refer tensor_filter_custom.h
 */
static void
pt_exit (void *_data, const GstTensorFilterProperties * prop)
{
  pt_data *data = _data;

  assert (data);
  free (data);
}

/**
 * @brief nnstreamer custom filter standard vmethod
 * Refer tensor_filter_custom.h
 */
static int
set_inputDim (void *_data, const GstTensorFilterProperties * prop,
    const GstTensorsInfo * in_info, GstTensorsInfo * out_info)
{
  pt_data *data = _data;
  int i, t;

  assert (data);
  assert (in_info);
  assert (out_info);

  data->num_audio_stream = in_info->num_tensors;

  /* input stream and sample-rate list */
  out_info->num_tensors = in_info->num_tensors + 1;

  for (t = 0; t < in_info->num_tensors; t++) {
    out_info->info[t].name = NULL;
    out_info->info[t].type = in_info->info[t].type;

    for (i = 0; i < NNS_TENSOR_RANK_LIMIT; i++) {
      out_info->info[t].dimension[i] = in_info->info[t].dimension[i];
    }
  }

  /* add sample-rate list */
  out_info->info[t].name = NULL;
  out_info->info[t].type = _NNS_INT32;

  out_info->info[t].dimension[0] = in_info->num_tensors;
  for (i = 1; i < NNS_TENSOR_RANK_LIMIT; ++i)
    out_info->info[t].dimension[i] = 1;

  return 0;
}

/**
 * @brief nnstreamer custom filter standard vmethod
 * Refer tensor_filter_custom.h
 */
static int
invoke (void *_data, const GstTensorFilterProperties * prop,
    const GstTensorMemory * input, GstTensorMemory * output)
{
  pt_data *data = _data;
  int t, sample_list_index;

  assert (data);

  sample_list_index = prop->input_meta.num_tensors;

  for (t = 0; t < prop->input_meta.num_tensors; t++) {
    /* copy input stream */
    memcpy (output[t].data, input[t].data, input[t].size);

    /* supposed dimension[1] is audio sample-rate */
    ((int *) output[sample_list_index].data)[t] =
        prop->input_meta.info[t].dimension[1];
  }

  return 0;
}

static NNStreamer_custom_class NNStreamer_custom_body = {
  .initfunc = pt_init,
  .exitfunc = pt_exit,
  .setInputDim = set_inputDim,
  .invoke = invoke,
};

/* The dyn-loaded object */
NNStreamer_custom_class *NNStreamer_custom = &NNStreamer_custom_body;
