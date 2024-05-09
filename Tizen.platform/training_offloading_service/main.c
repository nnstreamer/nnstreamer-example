/**
 * @file  main.c
 * @date  2 May 2024
 * @brief  Training offloading service app.
 * @see  https://github.com/nnstreamer/nnstreamer
 * @author  <hyunil46.park@samsung.com>
 * @bug  No known bugs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <ml-api-service.h>

#define MAX_STRING_LEN	2048

enum {
  CURRENT_STATUS_MAINMENU,
  CURRENT_STATUS_INPUT_FILENAME,
  CURRENT_STATUS_INPUT_RW_PATH,
};

int g_menu_state = CURRENT_STATUS_MAINMENU;
GMainLoop *loop;
static gchar g_config_path[MAX_STRING_LEN];
static gchar g_path[MAX_STRING_LEN];
ml_service_h service_h;
#define ML_SERVICE_EVENT_REPLY 4

/**
 * @brief Callback function for reply test.
 */
static void
_receive_trained_model_cb (ml_service_event_e event, ml_information_h event_data, void *user_data)
{
  switch ((int) event) {
    case ML_SERVICE_EVENT_REPLY:
      {
        g_warning ("Get ML_SERVICE_EVENT_REPLY and received trained model");
        break;
      }
    default:
      break;
  }
}

/**
 * @brief Callback function for sink node.
 */
static void
_sink_register_cb (ml_service_event_e event, ml_information_h event_data, void *user_data)
{
  ml_tensors_data_h data = NULL;
  double *output = NULL;
  size_t data_size = 0;
  int status, i;
  double result_data[4];
  char *output_node_name = NULL;

  switch (event) {
    case ML_SERVICE_EVENT_NEW_DATA:
      g_return_if_fail (event_data != NULL);

      status = ml_information_get (event_data, "name", (void **) &output_node_name);
      if (status != ML_ERROR_NONE)
        return;
      if (!output_node_name)
        return;

      status = ml_information_get (event_data, "data", &data);
      if (status != ML_ERROR_NONE)
        return;

      status = ml_tensors_data_get_tensor_data (data, 0, (void **) &output, &data_size);
      if (status != ML_ERROR_NONE)
        return;
      break;
    default:
      break;
  }

  if (output) {
    for (i = 0; i < 4; i++)
      result_data[i] = output[i];

    g_print ("name:%s >> [training_loss: %f, training_accuracy: %f, validation_loss: %f, validation_accuracy: %f]",
        output_node_name, result_data[0], result_data[1], result_data[2],
        result_data[3]);
  }
}

/**
 * @brief menu interface function
 */
static void 
input_filepath (gchar *src, gchar *path)
{
  gint len = 0;
  gsize size = 0;
  g_return_if_fail (path != NULL);

  len = strlen(path);
  if (len < 0 || len > MAX_STRING_LEN -1)
    return;
  size = g_strlcpy(src, path, MAX_STRING_LEN);
  if (len != size)
    return;
}

/**
 * @brief menu interface function
 */
void
main_menu ()
{
  g_print("\n");
	g_print("============================================================================\n");
	g_print("  Training Offloading Services Test (press q to quit) \n");
	g_print("----------------------------------------------------------------------------\n");
  g_print("a. Create \n");
  g_print("b. Set R/W path \n");
  g_print("c. Set callback for receiver \n");
  g_print("d. Set callback for sender \n");
  g_print("e. Start \n");
  g_print("f. Stop \n");
  g_print("g. End \n");
  g_print("q. Quit \n");
	g_print("============================================================================\n");  
}

/**
 * @brief menu interface function
 */
static void
quit_program ()
{
  g_main_loop_quit (loop);
}

/**
 * @brief menu interface function
 */
void
reset_menu_status (void)
{
	g_menu_state = CURRENT_STATUS_MAINMENU;
}

/**
 * @brief menu interface function
 */
static void
display_menu ()
{
  if (g_menu_state == CURRENT_STATUS_MAINMENU) {
    main_menu();
  } else if (g_menu_state == CURRENT_STATUS_INPUT_FILENAME) {
    g_print("*** input config file path.\n");
  } else if (g_menu_state == CURRENT_STATUS_INPUT_RW_PATH) {
    g_print("*** input path for reading or writing. e.g. /opt \n");
  } else {
    g_print("*** Unknown status. \n");
  }
  g_print(" >>> ");
}

/**
 * @brief interpret main menu
 */
static void
interpret_main_menu (char *cmd)
{
  int ret = ML_ERROR_NONE;
  if (!g_strcmp0 (cmd, "a")) {
    g_menu_state = CURRENT_STATUS_INPUT_FILENAME;
  } else if (!g_strcmp0 (cmd, "b")) {
    g_menu_state = CURRENT_STATUS_INPUT_RW_PATH;
  } else if (!g_strcmp0 (cmd, "c")) {
    ret = ml_service_set_event_cb (service_h, _sink_register_cb, NULL);
  } else if (!g_strcmp0 (cmd, "d")) {
    ret = ml_service_set_event_cb (service_h, _receive_trained_model_cb, NULL);
  } else if (!g_strcmp0 (cmd, "e")) {
    ret = ml_service_start (service_h);
  } else if (!g_strcmp0 (cmd, "f")) {
    ret = ml_service_stop (service_h);
  } else if (!g_strcmp0 (cmd, "g")) {
    ret = ml_service_destroy (service_h);
  } else if (!g_strcmp0 (cmd, "q")) {
    quit_program();
    return;
  }

  if (ret == ML_ERROR_NONE)
      g_print("*** Success *** \n");
  else
      g_print("*** Failed *** \n");
}

/**
 * @brief timeout for menu display
 */
gboolean
timeout_menu_display (void *data)
{
	display_menu ();
	return FALSE;
}

/**
 * @brief interpret function
 */
static void
interpret (gchar *cmd)
{
  switch (g_menu_state) {
    case CURRENT_STATUS_MAINMENU:
      interpret_main_menu (cmd);
      break;
    case CURRENT_STATUS_INPUT_FILENAME:
      input_filepath (g_config_path, cmd);
      reset_menu_status ();
      g_print("config path=%s\n", g_config_path);
      if (ML_ERROR_NONE == ml_service_new (g_config_path, &service_h))
        g_print("Success to create service(handle=%p)\n", service_h);
      else
        g_print("Failed to create service \n");
      break;
    case CURRENT_STATUS_INPUT_RW_PATH:
      input_filepath (g_path, cmd);
      reset_menu_status ();
      if (ML_ERROR_NONE == ml_service_set_information (service_h, "path", g_path))
        g_print ("Success to set path \n");
      else
        g_print ("Failed to set path \n");
      break;
    default:
      break;
  }
  g_timeout_add (100, timeout_menu_display, 0);
}

/**
 * @brief input function
 */
gboolean
input (GIOChannel *channel)
{
	gchar buf[MAX_STRING_LEN];
	ssize_t cnt;

	memset (buf, 0, MAX_STRING_LEN);
	cnt = read (0, (void *)buf, MAX_STRING_LEN);
	if (cnt == 0) return TRUE;
	buf[cnt - 1] = 0;

	interpret (buf);

	return TRUE;
}

/**
 * @brief Main function.
 */
int
main (int argc, char **argv)
{
  GIOChannel *stdin_channel;

  
  stdin_channel = g_io_channel_unix_new (0);
	g_io_channel_set_flags (stdin_channel, G_IO_FLAG_NONBLOCK, NULL);
	g_io_add_watch (stdin_channel, G_IO_IN, (GIOFunc)input, NULL);

  loop = g_main_loop_new (NULL, FALSE);

  g_print("running\n");
  display_menu ();
  g_main_loop_run (loop);
  g_print("exit training offloading services\n");
  g_main_loop_unref (loop);

  return 0;
}
