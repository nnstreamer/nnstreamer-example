/**
 * @file	main.h
 * @date	08 Feb 2022
 * @brief	example with TF-Lite model for low light image enhancement
 * @see		https://github.com/nnstreamer/nnstreamer
 * @author	Yelin Jeong <yelini.jeong@samsung.com>
 * @bug		This example runs well in emulator, but on the Tizen device it
 * 			takes more than 10 seconds.
 */

#ifndef __lowlightimageenhancementpipeline_H__
#define __lowlightimageenhancementpipeline_H__

#include <app.h>
#include <Elementary.h>
#include <system_settings.h>
#include <efl_extension.h>
#include <dlog.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "LowLightImageEnhancement"

#if !defined(PACKAGE)
#define PACKAGE "org.example.lowlightimageenhancementpipeline"
#endif

#endif /* __lowlightimageenhancementpipeline_H__ */
