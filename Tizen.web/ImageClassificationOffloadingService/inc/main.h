/**
 * @file	main.h
 * @date	04 Jul 2024
 * @brief	Tizen native service for hybrid application
 * @author	Yelin Jeong <yelini.jeong@samsung.com>
 * @bug		No known bugs.
 */

#ifndef __IMAGECLASSIFICATIONOFFLOADINGSERVICE_H__
#define __IMAGECLASSIFICATIONOFFLOADINGSERVICE_H__

#include <dlog.h>

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "image_classification_offloading_service"

#define REMOTE_APP_ID "EQmf4iSfpX.ImageClassificationOffloading"
#define PORT_NAME "MESSAGE_PORT"

#endif /* __IMAGECLASSIFICATIONOFFLOADINGSERVICE_H__ */
