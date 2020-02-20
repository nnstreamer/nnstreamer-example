/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if !defined(_MAIN_H_)
#define _MAIN_H_

#include <dlog.h>
#include <nnstreamer.h>

#if !defined(PACKAGE)
#define PACKAGE "org.example.objectdetection"
#endif

#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "objectdetection"

#define _DEBUG_MSG_LOG_BUFFER_SIZE_ 1024
#define DLOG_PRINT_DEBUG_MSG(fmt, args...) do { char _log_[_DEBUG_MSG_LOG_BUFFER_SIZE_]; \
    snprintf(_log_, _PRINT_MSG_LOG_BUFFER_SIZE_, fmt, ##args); \
    dlog_print(DLOG_DEBUG, LOG_TAG, _log_); } while (0)

#define DLOG_PRINT_ERROR(fun_name, error_code) dlog_print(DLOG_ERROR, LOG_TAG, \
        "%s() failed! Error: %s [code: %d]", \
        fun_name, get_error_message(error_code), error_code)

#define CHECK_ERROR(fun_name, error_code) if (error_code != 0) { \
    DLOG_PRINT_ERROR(fun_name, error_code); \
    }

#define CHECK_ERROR_AND_RETURN(fun_name, error_code) if (error_code != 0) { \
    DLOG_PRINT_ERROR(fun_name, error_code); \
    return; \
    }
CustomData* custom_data;

#endif
