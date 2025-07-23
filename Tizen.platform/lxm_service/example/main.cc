/**
 * @file  main.cc
 * @date  23 JULY 2025
 * @brief  Large Language Model service example using ML LXM Service API
 * @see  https://github.com/nnstreamer/nnstreamer
 * @author  <hyunil46.park@samsung.com>
 * @bug  No known bugs.
 */
#include <glib.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ml-api-service.h>
#include "ml-lxm-service.h"

#define MAX_STRING_LEN 4096

enum {
  CURRENT_STATUS_MAINMENU,
  CURRENT_STATUS_SESSION_CREATE,
  CURRENT_STATUS_SESSION_DESTROY,
  CURRENT_STATUS_SESSION_SET_INSTRUCTION,
  CURRENT_STATUS_SESSION_RESPOND,
  CURRENT_STATUS_SESSION_RESPOND_ASYNC,
  CURRENT_STATUS_PROMPT_CREATE,
  CURRENT_STATUS_PROMPT_APPEND_TEXT,
  CURRENT_STATUS_PROMPT_APPEND_INSTRUCTION,
  CURRENT_STATUS_PROMPT_DESTROY
};

int g_menu_state = CURRENT_STATUS_MAINMENU;
GMainLoop *loop;
ml_lxm_session_h g_session = NULL;   // Global session handle
ml_lxm_prompt_h g_prompt = NULL;     // Global prompt handle
char g_instructions[MAX_STRING_LEN]; // Buffer for instructions
char g_prompt_text[MAX_STRING_LEN];  // Buffer for prompt text

/**
 * @brief Token callback for async response
 */
static void token_handler(ml_service_event_e event, ml_information_h event_data,
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

      std::cout.write(static_cast<const char *>(_raw),
                      _size); /* korean output */
      std::cout.flush();
    }
  default:
    break;
  }
}

/**
 * @brief Main menu display
 */
void main_menu() {
  g_print("\n");
  g_print("================================================================\n");
  g_print("  ML LLM Service Test (press q to quit) \n");
  g_print("----------------------------------------------------------------\n");
  g_print("a. Session Create \n");
  g_print("b. Session Destroy \n");
  g_print("c. Session Set Instruction \n");
  g_print("d. Session Respond \n");
  g_print("e. Prompt Create \n");
  g_print("f. Prompt Destroy \n");
  g_print("g. Prompt Append Text \n");
  g_print("h. Prompt Append Instruction \n");
  g_print("q. Quit \n");
  g_print("================================================================\n");
}

/**
 * @brief Quit program
 */
static void quit_program() {
  // Cleanup before exit
  if (g_prompt) {
    ml_lxm_prompt_destroy(g_prompt);
    g_prompt = NULL;
  }
  if (g_session) {
    ml_lxm_session_destroy(g_session);
    g_session = NULL;
  }
  g_main_loop_quit(loop);
}

/**
 * @brief Reset menu status
 */
void reset_menu_status(void) { g_menu_state = CURRENT_STATUS_MAINMENU; }

/**
 * @brief Display current menu
 */
static void display_menu() {
  if (g_menu_state == CURRENT_STATUS_MAINMENU) {
    main_menu();
  } else if (g_menu_state == CURRENT_STATUS_SESSION_CREATE) {
    g_print("*** Enter config path: ");
  } else if (g_menu_state == CURRENT_STATUS_SESSION_SET_INSTRUCTION) {
    g_print("*** Enter new instructions: ");
  } else if (g_menu_state == CURRENT_STATUS_PROMPT_APPEND_TEXT) {
    g_print("*** Enter text to append: ");
  } else if (g_menu_state == CURRENT_STATUS_PROMPT_APPEND_INSTRUCTION) {
    g_print("*** Enter instruction to append: ");
  } else {
    g_print("*** Unknown status. \n");
    reset_menu_status();
  }
}

/**
 * @brief Interpret main menu commands
 */
static void interpret_main_menu(char cmd) {
  int ret = ML_ERROR_NONE;

  ml_lxm_generation_options_s options = {.temperature = 1.2, .max_tokens = 128};

  switch (cmd) {
  case 'a':
    if (g_session) {
      g_print("Session already exists!\n");
      break;
    }
    g_menu_state = CURRENT_STATUS_SESSION_CREATE;
    break;
  case 'b':
    if (!g_session) {
      g_print("No active session!\n");
      break;
    }
    ret = ml_lxm_session_destroy(g_session);
    if (ret == 0) {
      g_session = NULL;
      g_print("Session destroyed.\n");
    } else {
      g_print("Session destruction failed: %d\n", ret);
    }
    break;
  case 'c':
    if (!g_session) {
      g_print("Create a session first!\n");
      break;
    }
    g_menu_state = CURRENT_STATUS_SESSION_SET_INSTRUCTION;
    break;

  case 'd':
    if (!g_session || !g_prompt) {
      g_print("Create session and prompt first!\n");
      break;
    }

    ret = ml_lxm_session_respond(g_session, g_prompt, &options,
                                       token_handler, NULL);
    if (ret == 0) {
      g_print("\nAsync response started...\n");
    } else {
      g_print("Async response failed: %d\n", ret);
    }
    break;
  case 'e':
    if (g_prompt) {
      g_print("Prompt already exists! Destroy first.\n");
      break;
    }
    ret = ml_lxm_prompt_create(&g_prompt);
    if (ret == 0) {
      g_print("Prompt created.\n");
    } else {
      g_print("Prompt creation failed: %d\n", ret);
    }
    break;
  case 'f':
    if (!g_prompt) {
      g_print("No active prompt!\n");
      break;
    }
    ret = ml_lxm_prompt_destroy(g_prompt);
    if (ret == 0) {
      g_prompt = NULL;
      g_print("Prompt destroyed.\n");
    } else {
      g_print("Prompt destruction failed: %d\n", ret);
    }
    break;
  case 'g':
    if (!g_prompt) {
      g_print("Create a prompt first!\n");
      break;
    }
    g_menu_state = CURRENT_STATUS_PROMPT_APPEND_TEXT;
    break;

  case 'h':
    if (!g_prompt) {
      g_print("Create a prompt first!\n");
      break;
    }
    g_menu_state = CURRENT_STATUS_PROMPT_APPEND_INSTRUCTION;
    break;
  case 'q':
    quit_program();
    return;
  default:
    g_print("Invalid option!\n");
  }

  if (ret != ML_ERROR_NONE && cmd != 'q') {
    g_print("Operation failed with error: %d\n", ret);
  }
}

/**
 * @brief Handle input for non-main-menu states
 */
static void handle_special_input(const char *input) {
  int ret = ML_ERROR_NONE;

  switch (g_menu_state) {
  case CURRENT_STATUS_SESSION_CREATE:
    g_critical("Input: %s\n", input);
    ret = ml_lxm_session_create(&g_session, input, "Default instructions.");
    if (ret == 0) {
      g_print("Session created successfully.\n");
    } else {
      g_print("Session creation failed: %d\n", ret);
    }
    break;
  case CURRENT_STATUS_SESSION_SET_INSTRUCTION:
    ret = ml_lxm_session_set_instructions(g_session, input);
    if (ret == 0) {
      snprintf(g_instructions, MAX_STRING_LEN, "%s", input);
      g_print("Instructions updated.\n");
    } else {
      g_print("Failed to set instructions: %d\n", ret);
    }
    break;
  case CURRENT_STATUS_PROMPT_APPEND_TEXT:
    ret = ml_lxm_prompt_append_text(g_prompt, input);
    if (ret == 0) {
      snprintf(g_prompt_text, MAX_STRING_LEN, "%s", input);
      g_print("Text appended to prompt.\n");
    } else {
      g_print("Failed to append text: %d\n", ret);
    }
    break;
  case CURRENT_STATUS_PROMPT_APPEND_INSTRUCTION:
    ret = ml_lxm_prompt_append_instruction(g_prompt, input);
    if (ret == 0) {
      snprintf(g_prompt_text, MAX_STRING_LEN, "%s", input);
      g_print("Instruction appended to prompt.\n");
    } else {
      g_print("Failed to append instruction: %d\n", ret);
    }
    break;

  default:
    break;
  }

  reset_menu_status();
}

/**
 * @brief Input handler
 */
gboolean input_handler(GIOChannel *channel, GIOCondition condition,
                       gpointer data) {
  char buf[MAX_STRING_LEN];
  ssize_t cnt = read(STDIN_FILENO, buf, sizeof(buf) - 1);

  if (cnt <= 0)
    return TRUE; /* Keep watching */

  buf[cnt] = '\0';

  if (g_menu_state == CURRENT_STATUS_MAINMENU && cnt == 2) {
    interpret_main_menu(buf[0]);
  } else if (g_menu_state != CURRENT_STATUS_MAINMENU) {
    /* Remove newline if present */
    if (buf[cnt - 1] == '\n')
      buf[cnt - 1] = '\0';
    handle_special_input(buf);
  }

  display_menu();
  return TRUE;
}

/**
 * @brief Main function
 */
int main(int argc, char **argv) {
  loop = g_main_loop_new(NULL, FALSE);
  GIOChannel *stdin_channel = g_io_channel_unix_new(STDIN_FILENO);
  guint watch_id = g_io_add_watch(stdin_channel, static_cast<GIOCondition>(G_IO_IN | G_IO_HUP), input_handler, NULL);

  display_menu();
  g_main_loop_run(loop);

  if (g_prompt)
    ml_lxm_prompt_destroy(g_prompt);
  if (g_session)
    ml_lxm_session_destroy(g_session);
  if (watch_id > 0)
    g_source_remove(watch_id);
  if (stdin_channel)
    g_io_channel_unref(stdin_channel);

  return 0;
}
