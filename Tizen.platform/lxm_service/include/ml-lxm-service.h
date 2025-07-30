/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    ml-lxm-service.h
 * @date    23 JULY 2025
 * @brief   Machine Learning LXM(LLM, LVM, etc.) Service API
 * @see     https://github.com/nnstreamer/api
 * @author  Hyunil Park <hyunil46.park@samsung.com>
 * @bug     No known bugs except for NYI items
 */

#ifndef __ML_LXM_SERVICE_H__
#define __ML_LXM_SERVICE_H__

#include <stdlib.h>
#include <ml-api-service.h>
#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Enumeration for LXM service availability status.
 */
typedef enum
{
  ML_LXM_AVAILABILITY_AVAILABLE = 0,
  ML_LXM_AVAILABILITY_DEVICE_NOT_ELIGIBLE,
  ML_LXM_AVAILABILITY_SERVICE_DISABLED,
  ML_LXM_AVAILABILITY_MODEL_NOT_READY,
  ML_LXM_AVAILABILITY_UNKNOWN
} ml_lxm_availability_e;

/**
 * @brief Checks LXM service availability.
 * @param[out] status Current availability status.
 * @return ML_ERROR_NONE on success, error code otherwise.
 */
int ml_lxm_check_availability (ml_lxm_availability_e * status);

/**
 * @brief A handle for lxm session.
 */
typedef void *ml_lxm_session_h;

/**
 * @brief Creates an LXM session.
 * @param[out] session Session handle.
 * @param[in] config_path Path to configuration file.
 * @param[in] instructions Initial instructions (optional).
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_session_create (ml_lxm_session_h * session, const char *config_path, const char *instructions);

/**
 * @brief Destroys an LXM session.
 * @param[in] session Session handle.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_session_destroy (ml_lxm_session_h session);

/**
 * @brief A handle for lxm prompt.
 */
typedef void *ml_lxm_prompt_h;

/**
 * @brief Creates a prompt object.
 * @param[out] prompt Prompt handle.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_prompt_create (ml_lxm_prompt_h * prompt);

/**
 * @brief Appends text to a prompt.
 * @param[in] prompt Prompt handle.
 * @param[in] text Text to append.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_prompt_append_text (ml_lxm_prompt_h prompt, const char *text);

/**
 * @brief Appends an instruction to a prompt.
 * @param[in] prompt Prompt handle.
 * @param[in] instruction Instruction to append.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_prompt_append_instruction (ml_lxm_prompt_h prompt, const char *instruction);

/**
 * @brief Destroys a prompt object.
 * @param[in] prompt Prompt handle.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_prompt_destroy (ml_lxm_prompt_h prompt);

/**
 * @brief Sets runtime instructions for a session.
 * @param[in] session Session handle.
 * @param[in] instructions New instructions.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_session_set_instructions (ml_lxm_session_h session, const char *instructions);

/**
 * @brief Generation options for LXM responses.
 */
typedef struct
{
  double temperature; /* < Creativity control (0.0~2.0) */
  size_t max_tokens;  /* < Maximum tokens to generate */
} ml_lxm_generation_options_s;

/**
 * @brief Token streaming callback type.
 * @param token Generated token string.
 * @param user_data User-defined context.
 */
typedef void (*ml_lxm_token_cb) (ml_service_event_e event, ml_information_h event_data, void *user_data);

/**
 * @brief Generates an token-streamed response.
 * @param[in] session Session handle.
 * @param[in] prompt Prompt handle.
 * @param[in] options Generation parameters.
 * @param[in] token_callback Callback for each generated token.
 * @param[in] user_data User context passed to callback.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_session_respond (ml_lxm_session_h session, ml_lxm_prompt_h prompt, const ml_lxm_generation_options_s * options, ml_lxm_token_cb token_callback, void *user_data);

#ifdef __cplusplus
}
#endif
#endif
/* __ML_LXM_SERVICE_H__ */
