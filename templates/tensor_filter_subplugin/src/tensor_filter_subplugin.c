/**
 * GStreamer Tensor_Filter TEMPLATE Code
 * Copyright (C) 2019 MyungJoo Ham <myungjoo.ham@samsung.com>
 *
 * This is a template with no license requirements.
 * Writers may alter the license to anything they want.
 * The author hereby allows to do so.
 */
/**
 * @file	tensor_filter_subplugin.c
 * @date	11 Oct 2019
 * @brief	NNStreamer tensor-filter subplugin template
 * @see		http://github.com/nnsuite/nnstreamer
 * @author	MyungJoo Ham <myungjoo.ham@samsung.com>
 * @bug		No known bugs
 */

#include <string.h>
#include <glib.h>
#include <nnstreamer_plugin_api_filter.h>

void init_filter_TEMPLATE (void) __attribute__ ((constructor));
void fini_filter_TEMPLATE (void) __attribute__ ((destructor));

/**
 * @brief If you need to store session or model data,
 *        this is what you want to fill in.
 */
typedef struct
{
  gchar *model_path; /**< This is a sample. You may remove/change it */
} TEMPLATE_pdata;

static void TEMPLATE_close (const GstTensorFilterProperties * prop,
    void **private_data);

/**
 * @brief The standard tensor_filter callback
 */
static int
TEMPLATE_open (const GstTensorFilterProperties * prop, void **private_data)
{
  int err = 0;
  TEMPLATE_pdata *pdata;

  if (*private_data != NULL) {
    pdata = *private_data;
    if (strcmp (prop->model_file, pdata->model_path)) {
      TEMPLATE_close (prop, private_data);  /* "reopen" */
    } else {
      return 1;
    }
  }

  pdata = g_new0 (TEMPLATE_pdata, 1);
  if (pdata == NULL)
    return -1;

  *private_data = (void *) pdata;

  /** @todo Initialize your own framework or hardware here */

  pdata->model_path = g_strdup (prop->model_file);

  return err;
}

/**
 * @brief The standard tensor_filter callback
 */
static void
TEMPLATE_close (const GstTensorFilterProperties * prop, void **private_data)
{
  TEMPLATE_pdata *pdata;
  pdata = *private_data;

  /** @todo Close what you have opened/allocated with TEMPLATE_open */

  g_free (pdata->model_path);
  pdata->model_path = NULL;

  g_free (pdata);
  *private_data = NULL;
}

/**
 * @brief The standard tensor_filter callback for static input/output dimension.
 * @note If you want to support flexible/dynamic input/output dimension,
 *       read nnstreamer_plugin_api_filter.h and supply the
 *       setInputDimension callback.
 */
static int
TEMPLATE_getInputDim (const GstTensorFilterProperties * prop,
    void **private_data, GstTensorsInfo * info)
{
  /** @todo Configure info with the proper (static) input tensor dimension */
  return 0;
}

/**
 * @brief The standard tensor_filter callback for static input/output dimension.
 * @note If you want to support flexible/dynamic input/output dimension,
 *       read nnstreamer_plugin_api_filter.h and supply the
 *       setInputDimension callback.
 */
static int
TEMPLATE_getOutputDim (const GstTensorFilterProperties * prop,
    void **private_data, GstTensorsInfo * info)
{
  /** @todo Configure info with the proper (static) output tensor dimension */
  return 0;
}

/**
 * @brief The standard tensor_filter callback
 */
static int
TEMPLATE_invoke (const GstTensorFilterProperties * prop,
    void **private_data, const GstTensorMemory * input,
    GstTensorMemory * output)
{
  /** @todo Call your framework/hardware with the given input. */
  return 0;
}

static gchar filter_subplugin_TEMPLATE[] = "TEMPLATE";

static GstTensorFilterFramework NNS_support_TEMPLATE = {
  .name = filter_subplugin_TEMPLATE,
  .allow_in_place = FALSE,
  .allocate_in_invoke = FALSE,
  .run_without_model = FALSE,
  .invoke_NN = TEMPLATE_invoke,
  .getInputDimension = TEMPLATE_getInputDim,
  .getOutputDimension = TEMPLATE_getOutputDim,
  .open = TEMPLATE_open,
  .close = TEMPLATE_close,
};

/**@brief Initialize this object for tensor_filter subplugin runtime register */
void
init_filter_TEMPLATE (void)
{
  nnstreamer_filter_probe (&NNS_support_TEMPLATE);
}

/** @brief Destruct the subplugin */
void
fini_filter_TEMPLATE (void)
{
  nnstreamer_filter_exit (NNS_support_TEMPLATE.name);
}
