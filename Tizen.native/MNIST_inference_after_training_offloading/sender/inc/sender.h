/**
 * @file sender.h
 * @date 21 September 2023
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnstreamer/nnstreamer
 * @author Hyunil Park <hyunil46.park.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */
#ifndef __sender_H__
#define __sender_H__

#include <app.h>
#include <Elementary.h>
#include <system_settings.h>
#include <efl_extension.h>
#include <dlog.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "sender"

#if !defined(PACKAGE)
#define PACKAGE "org.example.sender"
#endif

#endif /* __sender_H__ */
