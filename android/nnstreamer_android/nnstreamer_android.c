/**
 * @file    nnstreamer_android.c
 * @date    07 Mar 2019
 * @brief   test case for gst_aprse_launch
 * @see     https://github.com/nnsuite/nnstreamer
 * @author  Jijoong Moon <jijoong.moon@samsung.com>
 * @bug     No known bugs except NYI.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <gst/gst.h>
#include <tensor_common.h>
#include <tensor_repo.h>

#ifndef DBG
#define DBG FALSE
#endif

#define _print_log(...) if (DBG) g_message (__VA_ARGS__)

/**
 * @brief Main function to evalute gst_parse_launch
 */
int
main (int argc, char *argv[])
{
  GstElement *pipeline;

  gst_init (&argc, &argv);

  pipeline =
      gst_parse_launch
      ("videotestsrc num-buffers=1 ! video/x-raw,format=RGB,width=280,height=40,framerate=0/1 ! tee name=t ! queue ! videoconvert ! video/x-raw, format=BGRx ! tensor_converter silent=TRUE ! filesink location=\"test.bgrx.log\" sync=true t. ! queue ! filesink location=\"test.rgb.log\" sync=true",
      NULL);

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_PLAYING);

  gst_element_set_state (GST_ELEMENT (pipeline), GST_STATE_NULL);

  gst_object_unref (GST_OBJECT (pipeline));

  return 0;
}
