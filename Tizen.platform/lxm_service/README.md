---
title: On device Machine LXM(LLM, LVM, etc.) Service API example
...

## On device Machine LXM(LLM, LVM, etc.) Service API example

This example demonstrates how to implement a Large Language Model (LLM) service using `ml-lxm-service`(unreleased).

## Overview
The sample application provides an interactive CLI(Command Line Interface) to:
- Create/destroy LXM sessions
- Set instructions
- Create/destroy prompts
- Append text/instructions to prompts
- Generate token-streamed responses



## Prerequisites
- `ml-api-service` and `flare` development packages installed on your target device

## Building the Example
### Build
```
meson setup build --prefix=${NNST_ROOT} --libdir=lib --bindir=bin --includedir=include
ninja -C build install
```
### For rpm
```
./gen-ml-lxm-service-rpm.sh
```
### Run
```
cp config.conf tokenlizer.json sflare_if_4bit_3b.bin ${PROJECT}/
./build/ml-lxm-service-example
```


## API Reference

#### LXM service availability status.
```cpp
typedef enum
{
  ML_LXM_AVAILABILITY_AVAILABLE = 0,
  ML_LXM_AVAILABILITY_DEVICE_NOT_ELIGIBLE,
  ML_LXM_AVAILABILITY_SERVICE_DISABLED,
  ML_LXM_AVAILABILITY_MODEL_NOT_READY,
  ML_LXM_AVAILABILITY_UNKNOWN
} ml_lxm_availability_e;
```
#### Availability Check
```cpp
/**
 * @brief Checks LXM service availability.
 * @param[out] status Current availability status.
 * @return ML_ERROR_NONE on success, error code otherwise.
 */
int ml_lxm_check_availability (ml_lxm_availability_e * status);
```

### Data Type
```cpp
/**
 * @brief Token streaming callback type.
 * @param token Generated token string.
 * @param user_data User-defined context.
 */
typedef void (*ml_lxm_token_cb)(ml_service_event_e event, ml_information_h event_data, void *user_data);

/**
 * @brief Generation options for LXM responses.
 */
typedef struct {
  double temperature;
  size_t max_tokens;
} ml_lxm_generation_options_s;
```
#### Session Management
```cpp
/**
 * @brief Creates an LXM session.
 * @param[out] session Session handle.
 * @param[in] config_path Path to configuration file.
 * @param[in] instructions Initial instructions (optional).
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_session_create(ml_lxm_session_h *session, const char *config_path, const char *instructions);

/**
 * @brief Destroys an LXM session.
 * @param[in] session Session handle.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_session_destroy(ml_lxm_session_h session);

/**
 * @brief Sets runtime instructions for a session.
 * @param[in] session Session handle.
 * @param[in] instructions New instructions.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_session_set_instructions(ml_lxm_session_h session, const char *instructions);
```

#### Prompt Handling
```cpp
/**
 * @brief Creates a prompt object.
 * @param[out] prompt Prompt handle.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_prompt_create(ml_lxm_prompt_h *prompt);

/**
 * @brief Destroys a prompt object.
 * @param[in] prompt Prompt handle.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_prompt_destroy(ml_lxm_prompt_h prompt);

/**
 * @brief Appends text to a prompt.
 * @param[in] prompt Prompt handle.
 * @param[in] text Text to append.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_prompt_append_text(ml_lxm_prompt_h prompt, const char *text);

/**
 * @brief Appends an instruction to a prompt.
 * @param[in] prompt Prompt handle.
 * @param[in] instruction Instruction to append.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_prompt_append_instruction(ml_lxm_prompt_h prompt, const char *instruction);
```

#### Response Generation
```cpp
/**
 * @brief Generates an token-streamed response.
 * @param[in] session Session handle.
 * @param[in] prompt Prompt handle.
 * @param[in] options Generation parameters.
 * @param[in] token_callback Callback for each generated token.
 * @param[in] user_data User context passed to callback.
 * @return ML_ERROR_NONE on success.
 */
int ml_lxm_session_respond(
  ml_lxm_session_h session,
  ml_lxm_prompt_h prompt,
  const ml_lxm_generation_options_s *options,
  ml_lxm_token_cb token_cb,
  void *user_data
);
```
#### Error Codes
- ML_ERROR_NONE: Operation successful
- ML_ERROR_INVALID_PARAMETER: Invalid parameters detected
- ML_ERROR_OUT_OF_MEMORY: Memory allocation failure
- ML_ERROR_IO_ERROR: File/DB operation failure


## Sample Code Explanation

### Configuration file
```
{
    "single" :
    {
        "framework" : "flare",
        "model" : ["sflare_if_4bit_3b.bin"],
        "adapter" : ["history_lora.bin"],
        "custom" : "tokenizer_path:tokenizer.json,backend:CPU,output_size:1024,model_type:3B,data_type:W4A32",
        "invoke_dynamic" : "true",
    }
}
```

### Key Components
```cpp
#include <ml-api-service.h>
#include "ml-lxm-service.h"

// Global handles
ml_lxm_session_h g_session = NULL;
ml_lxm_prompt_h g_prompt = NULL;

```
### Main Workflow
1. Session Creation
```cpp
ret = ml_lxm_session_create(&g_session, config_path, "Default instructions");
```
2. Prompt Handling
```cpp
ml_lxm_prompt_create(&g_prompt);
ml_lxm_prompt_append_text(g_prompt, "Explain quantum computing");
```
3. Response Generation
```cpp
ml_lxm_generation_options_s options = {
  .temperature = 1.2,
  .max_tokens = 128
};

ml_lxm_session_respond(
  g_session,
  g_prompt,
  &options,
  token_handler,
  NULL
);
```
4. Token Callback
```cpp
static void token_handler(
  ml_service_event_e event,
  ml_information_h event_data,
  void *user_data
) {
  /* Process tokens here */
}
```
5. Cleanup
```cpp
ml_lxm_prompt_destroy(g_prompt);
ml_lxm_session_destroy(g_session);
```
### Full Example Snippet
```cpp
#include <ml-api-service.h>
#include "ml-lxm-service.h"

static void token_handler(ml_service_event_e event,
                          ml_information_h event_data,
                          void *user_data);
int main() {
  ml_lxm_session_h session;
  ml_lxm_prompt_h prompt;

  // 1. Create session
  ml_lxm_session_create(&session, "/path/to/config", NULL);

  // 2. Create prompt
  ml_lxm_prompt_create(&prompt);
  ml_lxm_prompt_append_text(prompt, "Hello AI");

  // 3. Generate response
  ml_lxm_generation_options_s options = {1.0, 50};
  ml_lxm_session_respond(session, prompt, &options, token_handler, NULL);

  // 4. Cleanup
  ml_lxm_prompt_destroy(prompt);
  ml_lxm_session_destroy(session);

  return 0;
}

static void token_handler(ml_service_event_e event,
                          ml_information_h event_data,
                          void *user_data) {
  ml_tensors_data_h data = NULL;
  void *_raw = NULL;
  size_t _size = 0;
  int ret;

  switch (event) {
  case ML_SERVICE_EVENT_NEW_DATA:
    if (event_data != NULL) {

      ret = ml_information_get(event_data, "data", &data);
      if (ret != ML_ERROR_NONE) {
        g_print("Failed to get data from event_data\n");
        return;
      }

      ret = ml_tensors_data_get_tensor_data(data, 0U, &_raw, &_size);
      if (ret != ML_ERROR_NONE) {
        g_print("Failed to get tensor data\n");
        return;
      }

      std::cout.write(static_cast<const char *>(_raw), _size);
      std::cout.flush();
    }
  default:
    break;
  }
}
```
