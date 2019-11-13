/**
 * @file main.h
 * @date 13 November 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#if !defined(_MAIN_H_)
#define _MAIN_H_

#include <dlog.h>
#include <glib.h>
#include <nnstreamer.h>

#if !defined(PACKAGE)
#define PACKAGE "sec.odl.tizen_camera_nnstreamer"
#endif

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "NNSTREAMER_CAMERA"

#endif
