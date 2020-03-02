/**
 * @file main.h
 * @date 28 Feb 2020
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author Parichay Kapoor <pk.kapoor@samsung.com>
 * @bug No known bugs except for NYI items
 */

#if !defined(_MAIN_H_)
#define _MAIN_H_

#include <dlog.h>
#include <efl_extension.h>
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

Ecore_Pipe * data_output_pipe;

#endif
