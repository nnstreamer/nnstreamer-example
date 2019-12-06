/**
 * @file my_file_selector.c
 * @date 06 December 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#include "main.h"
#include "my_file_selector.h"

#include <storage.h>

#define BUFLEN 256

/**
 * @brief Navigates the file selector to the parent directory.
 * @details Called after selecting ".." to get to the parent directory.
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in EFL API.
 *
 * @param data The data passed to the function(not used here)
 * @param obj The object for which the 'clicked' event was triggered
 * @param event_info Additional event information(not used here)
 */
void
_fs_back_file_cb(void *data, Evas_Object * obj, void *event_info)
{
  /* Unmark the clicked main menu button */
  const Eina_List *list = elm_list_selected_items_get(obj);
  elm_list_item_selected_set(list->data, EINA_FALSE);

  /* If the last symbol of the directory path is "/" then remove it. */
  int len = strlen(file_selector_data.dir_path);

  if (file_selector_data.dir_path[len - 1] == '/')
    memset(file_selector_data.dir_path + len - 1, '\0', BUFLEN - len - 1);

  /* Set maximal achievable parent directory to media storage. */
  int is_max_parent =
      strncmp(file_selector_data.dir_path, file_selector_data.root_path,
      strlen(file_selector_data.dir_path));

  if (!is_max_parent)
    return;

  /* Find the last occurrence of the "/" symbol in the directory path. */
  int last_slash =
      strrchr(file_selector_data.dir_path, '/') - file_selector_data.dir_path;

  if (last_slash <= 0)
    return;

  /* Clear the list with files and directories. */
  elm_list_clear(obj);

  /* Change the directory path to the parent directory. */
  file_selector_data.dir_path[last_slash] = '\0';
  elm_object_text_set(file_selector_data.folder_path,
      file_selector_data.dir_path);

  /* Fill the list with files and directories found in the parent directory. */
  fill_file_list(obj);
}

/**
 * @brief This function invokes a callback defined by the user for the event
 *        of selecting a file in the file selecting pop-up. It passes the path
 *        to the selected file to the callback while invoking it.
 * @details Called after selecting a file in the file selecting pop-up.
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in EFL API.
 *
 * @param data The data passed to the function(not used here)
 * @param obj The object for which the 'clicked' event was triggered
 * @param event_info Additional event information
 */
void
_fs_selected_file_cb(void *data, Evas_Object * obj, void *event_info)
{
  /* Unmark the clicked main menu button */
  const Eina_List *list = elm_list_selected_items_get(obj);
  elm_list_item_selected_set(list->data, EINA_FALSE);

  /* Get the path to the file selected in the pop-up. */
  char full_path[BUFLEN];

        /**
	 * event_info contains the full path to the selected file or NULL
	 * if none was selected(or cancel was pressed).
	 */
  const char *file_name = elm_object_item_text_get(event_info);
  snprintf(full_path, BUFLEN, "%s/%s", file_selector_data.dir_path, file_name);

  /* Check the file extension(whether it's JPEG). */
  char *ext = NULL;

  if (file_name)
    ext = strrchr(file_name, '.');

  if (NULL != ext && (!strcmp(ext + 1, file_selector_data.file_extension))) {
    dlog_print(DLOG_DEBUG, LOG_TAG, "Selected file: %s.", file_name);
    evas_object_hide(file_selector_data.file_popup);

    /* Invoke function used when the file was selected. */
   (*file_selector_data.selected_callback)(full_path,
        file_selector_data.extra_data);
  }
}

/**
 * @brief Compares two files/directories names during list creation in order to sort them.
 * @details Directories has the priority, they are placed higher on the list.
 *          Files are placed after all directories.
 *
 * @param data1 The first file/directory data
 * @param data2 The second file/directory data
 */
int
_compare_fun_cb(const void *data1, const void *data2)
{
  Elm_Object_Item *item1 = (Elm_Object_Item *) data1;
  Elm_Object_Item *item2 = (Elm_Object_Item *) data2;

  const char *string1 = elm_object_item_part_text_get(item1, NULL);
  const char *string2 = elm_object_item_part_text_get(item2, NULL);

  int len1 = strlen(string1);
  int len2 = strlen(string2);

  const char *last1 = string1 + len1 - 1;
  const char *last2 = string2 + len2 - 1;

  if (*last1 == '/') {
    if (*last2 == '/')
      return 0;
    else
      return -1;
  } else if (*last2 == '/')
    return 1;
  else
    return 0;
}

/**
 * @brief Navigates the file selector to the chosen directory.
 * @details Called after selecting a directory in the file selecting pop-up.
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in EFL API.
 *
 * @param data The data passed to the function(not used here)
 * @param obj The object for which the 'clicked' event was triggered
 * @param event_info Additional event information
 */
void
_fs_to_selected_dir_cb(void *data, Evas_Object * obj, void *event_info)
{
  /* Create a full path to the selected directory. */
  char full_path[BUFLEN];
  const char *file_name = elm_object_item_text_get(event_info);
  snprintf(full_path, BUFLEN, "%s/%s", file_selector_data.dir_path, file_name);

  snprintf(file_selector_data.dir_path, BUFLEN, "%s", full_path);
  dlog_print(DLOG_DEBUG, LOG_TAG, "%s", file_selector_data.dir_path);

  /* Fill the list with the files and directories contained in the selected directory. */
  elm_list_clear(obj);
  fill_file_list(obj);

  int len = strlen(file_selector_data.dir_path);

  if (file_selector_data.dir_path[len - 1] == '/')
    memset(file_selector_data.dir_path + len - 1, '\0', BUFLEN - len - 1);

  elm_object_text_set(file_selector_data.folder_path,
      file_selector_data.dir_path);
}

/**
 * @brief Fills the file selector list with the files and directories contained
 *        in the directory the file selector is currently in.
 *
 * @param file_list The given file list
 */
void
fill_file_list(Evas_Object * file_list)
{
  /* Open the directory stream to get its files and directories. */
  DIR *const pDir = opendir(file_selector_data.dir_path);
  if (pDir == NULL) {
    int err = errno;
    dlog_print(DLOG_ERROR, LOG_TAG,
        "Could not open directory '%s', error: %s [%d]",
        file_selector_data.dir_path, strerror(err), err);
    PRINT_MSG("Could not open directory '%s', error: %s [%d]",
        file_selector_data.dir_path, strerror(err), err);
    return;
  }


  /* Find all files and directories contained in the current directory. */
  struct dirent *ent = NULL;
  char * dir_name = NULL;

  while ((ent = readdir(pDir)) != NULL) {
    dir_name = strdup(ent->d_name);
    if (ent->d_type == DT_DIR && (strncmp(ent->d_name, ".", 1) != 0)) {
      /* Add directory to the list. */
      elm_list_item_sorted_insert(file_list, strcat(dir_name,
              "/"), NULL, NULL, _fs_to_selected_dir_cb, NULL, _compare_fun_cb);
    } else if (ent->d_type == DT_REG) {
      /* Add file to the list. */
      elm_list_item_sorted_insert(file_list, dir_name, NULL, NULL,
          _fs_selected_file_cb, NULL, _compare_fun_cb);
    }
    free(dir_name);
  }

  /* Add ".." button to enable navigating to the parent directories. */
  elm_list_item_prepend(file_list, "..", NULL, NULL, _fs_back_file_cb, NULL);

  closedir(pDir);
}

/**
 * @brief Assigns the ID of the internal storage to the variable passed
 *        as the user data to the callback.
 * @details Called for every storage supported by the device.
 * @remarks This function matches the storage_device_supported_cb() signature
 *          defined in the storage-expand.h header file.
 *
 * @param storage_id  The unique ID of the detected storage
 * @param type        The type of the detected storage
 * @param state       The current state of the detected storage(not used here)
 * @param path        The absolute path to the root directory of the detected
 *                    storage. This argument is not used in this case.
 * @param user_data   The user data passed via void pointer
 *
 * @return @c true to continue iterating over supported storages, @c false to
 *         stop the iteration.
 */
static bool
_storage_info_cb(int storage_id, storage_type_e type, storage_state_e state,
    const char *path, void *user_data)
{
  int *id = (int *) user_data;

  if (STORAGE_TYPE_INTERNAL == type) {
    if (NULL != id)
      *id = storage_id;

    /* Internal storage found. Stop iterating. */
    return false;
  }

  return true;
}

/**
 * @brief Gets the internal storage ID.
 *
 * @param id A pointer to the integer where the internal storage ID will be stored
 * @return The value of the error_code
 */
static int
_get_internal_storage_id(int *id)
{
  int error_code =
      storage_foreach_device_supported(_storage_info_cb, (void *) id);
  CHECK_ERROR("storage_foreach_device_supported", error_code);

  return error_code;
}

/**
 * @brief Sets the extension that the file selector will look for.
 *
 * @param extension The extension that the file selector will look for
 */
void
set_file_extension(const char *extension)
{
  file_selector_data.file_extension = extension;
}

/**
 * @brief Sets the callback that will be invoked when a file will be selected.
 *
 * @param selected_callback The callback that will be invoked when a file will be selected
 */
void
set_file_selected_callback(file_selector_selected_cb selected_callback)
{
  file_selector_data.selected_callback = selected_callback;
}

/**
 * @brief Creates a pop-up for choosing a file.
 *
 * @param file_popup The object used as the pop-up window
 * @param selected_callback The callback function which will be invoked when
 *                          the file is chosen
 * @param data The data which will be passed to the callback(not used here)
 */
Evas_Object *
create_file_popup(Evas_Object * file_popup,
    file_selector_selected_cb selected_callback, void *data)
{
  /* Create an element inside window for displaying list of files. */
  file_selector_data.file_popup = elm_win_inwin_add(file_popup);
  elm_layout_theme_set(file_selector_data.file_popup, NULL, NULL, "minimal");
  file_selector_data.selected_callback = selected_callback;
  file_selector_data.extra_data = data;

  /* Create a box for the file list. */
  Evas_Object *box = elm_box_add(file_selector_data.file_popup);
  evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
  elm_win_inwin_content_set(file_selector_data.file_popup, box);
  elm_box_padding_set(box, 0.0, 0.0);
  evas_object_show(box);

  /* Get the initial directory for selecting file: */

  /* 1. Get internal storage id. */
  int internal_storage_id = -1;
  int error_code = _get_internal_storage_id(&internal_storage_id);
  if (STORAGE_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("_get_internal_storage_id", error_code);
    return NULL;
  }

  /* 2. Get internal storage root directory. */
  char *root_directory = NULL;
  error_code =
      storage_get_root_directory(internal_storage_id, &root_directory);
  if (STORAGE_ERROR_NONE != error_code) {
    DLOG_PRINT_ERROR("_get_internal_storage_id", error_code);
    return NULL;
  }

  snprintf(file_selector_data.dir_path, BUFLEN, "%s", root_directory);
  snprintf(file_selector_data.root_path, BUFLEN, "%s", root_directory);
  free(root_directory);

  file_selector_data.folder_path = elm_label_add(box);
  elm_object_text_set(file_selector_data.folder_path,
      file_selector_data.dir_path);
  elm_box_pack_start(box, file_selector_data.folder_path);
  evas_object_show(file_selector_data.folder_path);

  /* Create a list used for displaying files and directories found inside the current directory. */
  Evas_Object *file_list = elm_list_add(file_selector_data.file_popup);
  evas_object_size_hint_weight_set(file_list, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_size_hint_align_set(file_list, EVAS_HINT_FILL, EVAS_HINT_FILL);
  elm_box_padding_set(box, 0.0, 0.0);
  elm_box_pack_end(box, file_list);
  evas_object_show(file_list);

  /* Fill the list with the files and directories found inside the current directory. */
  fill_file_list(file_list);

  return file_selector_data.file_popup;
}
