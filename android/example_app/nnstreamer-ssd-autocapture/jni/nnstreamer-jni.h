/**
 * @file	nnstreamer-jni.h
 * @date	1 April 2019
 * @brief	Common definition for tensor stream example
 * @author	Jaeyun Jung <jy1210.jung@samsung.com>
 * @bug		No known bugs
 */

#ifndef __NNSTREAMER_JNI_H__
#define __NNSTREAMER_JNI_H__

#include <android/log.h>
#include <gst/gst.h>

#define TAG_NAME "NNStreamer"

#define nns_logd(...) \
    __android_log_print (ANDROID_LOG_DEBUG, TAG_NAME, __VA_ARGS__)

#define nns_logi(...) \
    __android_log_print (ANDROID_LOG_INFO, TAG_NAME, __VA_ARGS__)

#define nns_loge(...) \
    __android_log_print (ANDROID_LOG_ERROR, TAG_NAME, __VA_ARGS__)

/**
* @brief Information for pipeline info.
*/
typedef struct _NNSPipelineInfo
{
  gint id;                      /**< Id for the pipeline */
  const gchar *name;            /**< Pipeline name */
  const gchar *description;     /**< Pipeline description */

  gboolean (*init) (void);
  void (*free) (void);
  gboolean (*get_name) (gchar **name, const gint option);
  gboolean (*get_description) (gchar **desc, const gint option);
  gboolean (*prepare_pipeline) (const gint option);
  gboolean (*launch_pipeline) (GstElement **pipeline, const gint option);
} NNSPipelineInfo;

#ifdef __cplusplus
extern "C"
{
#endif

gboolean nns_register_pipeline(NNSPipelineInfo *info);

#ifdef __cplusplus
}
#endif

#endif /* __NNSTREAMER_JNI_H__ */
