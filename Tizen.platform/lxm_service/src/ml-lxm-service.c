/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    ml-lxm-service.c
 * @date    23 JULY 2025
 * @brief   Machine Learning LXM(LLM, LVM, etc.) Service API
 * @see     https://github.com/nnstreamer/api
 * @author  Hyunil Park <hyunil46.park@samsung.com>
 * @bug     No known bugs except for NYI items
 */
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <ml-api-service.h>
#include "ml-lxm-service.h"

/**
 * @brief Internal structure for the session.
 */
typedef struct
{
  ml_service_h service_handle;
  char *config_path;
  char *instructions;
} ml_lxm_session_internal;

/**
 * @brief Internal structure for the prompt.
 */
typedef struct
{
  GString *text;
} ml_lxm_prompt_internal;

/**
 * @brief Checks LXM service availability.
 * @param[out] status Current availability status.
 * @return ML_ERROR_NONE on success, error code otherwise.
 */
int
ml_lxm_check_availability (ml_lxm_availability_e * status)
{
  *status = ML_LXM_AVAILABILITY_AVAILABLE;

  return ML_ERROR_NONE;
}

/**
 * @brief Creates an LXM session.
 * @param[out] session Session handle.
 * @param[in] config_path Path to configuration file.
 * @param[in] instructions Initial instructions (optional).
 * @return ML_ERROR_NONE on success.
 */
int
ml_lxm_session_create (ml_lxm_session_h * session, const char *config_path,
    const char *instructions)
{
  if (!session || !config_path)
    return ML_ERROR_INVALID_PARAMETER;

  ml_lxm_session_internal *s = g_malloc0 (sizeof (ml_lxm_session_internal));
  if (!s)
    return ML_ERROR_OUT_OF_MEMORY;

  s->config_path = g_strdup (config_path);
  s->instructions = instructions ? g_strdup (instructions) : NULL;

  int ret = ml_service_new (s->config_path, &s->service_handle);
  if (ret != ML_ERROR_NONE) {
    g_free (s->config_path);
    g_free (s->instructions);
    g_free (s);
    return ret;
  }

  *session = s;

  return ML_ERROR_NONE;
}

/**
 * @brief Destroys an LXM session.
 * @param[in] session Session handle.
 * @return ML_ERROR_NONE on success.
 */
int
ml_lxm_session_destroy (ml_lxm_session_h session)
{
  if (!session)
    return ML_ERROR_INVALID_PARAMETER;

  ml_lxm_session_internal *s = (ml_lxm_session_internal *) session;

  ml_service_destroy (s->service_handle);

  g_free (s->config_path);
  g_free (s->instructions);
  g_free (s);

  return ML_ERROR_NONE;
}

/**
 * @brief Creates a prompt object.
 * @param[out] prompt Prompt handle.
 * @return ML_ERROR_NONE on success.
 */
int
ml_lxm_prompt_create (ml_lxm_prompt_h * prompt)
{
  if (!prompt)
    return ML_ERROR_INVALID_PARAMETER;

  ml_lxm_prompt_internal *p = g_malloc0 (sizeof (ml_lxm_prompt_internal));
  if (!p)
    return ML_ERROR_OUT_OF_MEMORY;

  p->text = g_string_new ("");
  *prompt = p;

  return ML_ERROR_NONE;
}

/**
 * @brief Destroys a prompt object.
 * @param[in] prompt Prompt handle.
 * @return ML_ERROR_NONE on success.
 */
int
ml_lxm_prompt_destroy (ml_lxm_prompt_h prompt)
{
  if (!prompt)
    return ML_ERROR_INVALID_PARAMETER;

  ml_lxm_prompt_internal *p = (ml_lxm_prompt_internal *) prompt;
  g_string_free (p->text, TRUE);
  g_free (p);

  return ML_ERROR_NONE;
}

/**
 * @brief Appends text to a prompt.
 * @param[in] prompt Prompt handle.
 * @param[in] text Text to append.
 * @return ML_ERROR_NONE on success.
 */
int
ml_lxm_prompt_append_text (ml_lxm_prompt_h prompt, const char *text)
{
  if (!prompt || !text)
    return ML_ERROR_INVALID_PARAMETER;

  ml_lxm_prompt_internal *p = (ml_lxm_prompt_internal *) prompt;
  g_string_append (p->text, text);

  return ML_ERROR_NONE;
}

/**
 * @brief Appends an instruction to a prompt.
 * @param[in] prompt Prompt handle.
 * @param[in] instruction Instruction to append.
 * @return ML_ERROR_NONE on success.
 */
int
ml_lxm_prompt_append_instruction (ml_lxm_prompt_h prompt,
    const char *instruction)
{
  return ml_lxm_prompt_append_text (prompt, instruction);
}

/**
 * @brief Sets runtime instructions for a session.
 * @param[in] session Session handle.
 * @param[in] instructions New instructions.
 * @return ML_ERROR_NONE on success.
 */
int
ml_lxm_session_set_instructions (ml_lxm_session_h session,
    const char *instructions)
{
  if (!session)
    return ML_ERROR_INVALID_PARAMETER;

  ml_lxm_session_internal *s = (ml_lxm_session_internal *) session;
  g_free (s->instructions);
  s->instructions = instructions ? g_strdup (instructions) : NULL;

  return ML_ERROR_NONE;
}

/**
 * @brief Generates an token-streamed response.
 * @param[in] session Session handle.
 * @param[in] prompt Prompt handle.
 * @param[in] options Generation parameters.
 * @param[in] token_callback Callback for each generated token.
 * @param[in] user_data User context passed to callback.
 * @return ML_ERROR_NONE on success.
 */
int
ml_lxm_session_respond (ml_lxm_session_h session,
    ml_lxm_prompt_h prompt,
    const ml_lxm_generation_options_s * options,
    ml_lxm_token_cb token_callback, void *user_data)
{
  int ret = ML_ERROR_NONE;

  if (!session || !prompt || !token_callback)
    return ML_ERROR_INVALID_PARAMETER;

  ml_lxm_session_internal *s = (ml_lxm_session_internal *) session;
  ml_lxm_prompt_internal *p = (ml_lxm_prompt_internal *) prompt;

  GString *full_input = g_string_new ("");
  if (s->instructions && s->instructions[0] != '\0') {
    g_string_append (full_input, s->instructions);
    g_string_append (full_input, "\n");
  }
  g_string_append (full_input, p->text->str);

  /* Prepare input data */
  ml_tensors_info_h input_info = NULL;
  ml_tensors_data_h input_data = NULL;

  ret = ml_service_set_event_cb (s->service_handle, token_callback, user_data);
  if (ret != ML_ERROR_NONE)
    goto error;

  /* Get input information */
  ret = ml_service_get_input_information (s->service_handle, NULL, &input_info);
  if (ret != ML_ERROR_NONE)
    goto error;

  /* Create input data */
  ret = ml_tensors_data_create (input_info, &input_data);
  if (ret != ML_ERROR_NONE) {
    goto error;
  }

  /* Set tensor data */
  ret =
      ml_tensors_data_set_tensor_data (input_data, 0U, full_input->str,
      full_input->len);
  if (ret != ML_ERROR_NONE) {
    goto error;
  }

  /* Send request */
  ret = ml_service_request (s->service_handle, NULL, input_data);

error:
  if (input_info)
    ml_tensors_info_destroy (input_info);
  if (input_data)
    ml_tensors_data_destroy (input_data);
  if (full_input)
    g_string_free (full_input, TRUE);

  return ret;
}
