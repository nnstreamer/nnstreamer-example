/**
 * @file view.c
 * @date 06 December 2019
 * @brief TIZEN Native Example App with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author HyoungJoo Ahn <hello.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

#include "main.h"
#include "view.h"
#include "data.h"

#define BUFLEN 256

/* Display for the messages emitted from the application */
Evas_Object *GLOBAL_DEBUG_BOX;

/**
 * @brief Adds a log message to the debug box.
 *
 * @param text The message text
 */
void
_add_entry_text(const char *text)
{
  Evas_Coord c_y;

  elm_entry_entry_append(GLOBAL_DEBUG_BOX, text);
  elm_entry_entry_append(GLOBAL_DEBUG_BOX, "<br>");
  elm_entry_cursor_end_set(GLOBAL_DEBUG_BOX);
  elm_entry_cursor_geometry_get(GLOBAL_DEBUG_BOX, NULL, &c_y, NULL, NULL);
  elm_scroller_region_show(GLOBAL_DEBUG_BOX, 0, c_y, 0, 0);
}

/**
 * @brief The UI Elements
 */
static struct view_info
{
  Evas_Object *win;
  Evas_Object *conform;
  Evas_Object *navi;
  Evas_Object *buttons[2];
} s_info = {
  .win = NULL,
  .conform = NULL,
  .navi = NULL,
  .buttons = {NULL},};


/**
 * @brief Called when the current view is popped.
 * @remarks This function matches the Elm_Naviframe_Item_Pop_Cb() signature
 *          defined in the EFL API.
 *
 * @param data The data passed to the callback(not used here)
 * @param item The object to pop(not used here)
 *
 * @return @c EINA_FALSE to cancel the item popping request
 */
Eina_Bool
_pop_cb(void *data, Elm_Object_Item * item)
{
  elm_win_lower(s_info.win);
  return EINA_FALSE;
}

/**
 * @brief Pops a navi that is on top of the stack
 */
void
_pop_navi()
{
  elm_naviframe_item_pop(s_info.navi);
}

/**
 * @brief Sets the 'disabled' state of a button.
 *
 * @param button The button number in array
 * @param disabled The state to put in in: @c EINA_TRUE for
 *        disabled, @c EINA_FALSE for enabled
 */
void
_disable_button(app_button button, Eina_Bool disabled)
{
  elm_object_disabled_set(s_info.buttons[button], disabled);
}

/**
 * @brief Creates one of the application button, defined in app_button.
 *
 * @param button The button to create
 * @param display The parent object the button will be added to
 * @param name The text that will be displayed on the button
 * @param callback The callback function that should be invoked when
 *                 the button is clicked
 *
 */
void
_create_button(app_button button, Evas_Object * display, char *name,
    void *callback)
{
  s_info.buttons[button] =
      _new_button((void *) button, display, name, callback);
}

/**
 * @brief Creates a new button.
 *
 * @param data The data passed to the callback
 * @param display The parent object the button will be added to
 * @param name The text that will be displayed on the button
 * @param callback The callback function that should be invoked when
 *                 the button is clicked
 *
 * @return The newly created button object
 */
Evas_Object *
_new_button(void *data, Evas_Object * display, char *name, void *callback)
{
  Evas_Object *bt = elm_button_add(display);
  elm_object_text_set(bt, name);
  evas_object_smart_callback_add(bt, "clicked", (Evas_Smart_Cb) callback,
      data);
  evas_object_size_hint_weight_set(bt, EVAS_HINT_EXPAND, 0.0);
  evas_object_size_hint_align_set(bt, EVAS_HINT_FILL, EVAS_HINT_FILL);
  elm_box_pack_end(display, bt);
  evas_object_show(bt);
  return bt;
}

/**
 * @brief Resets the debug display.
 * @details Called when the 'Reset' button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() type signature
 *          defined in EFL API.
 *
 * @param data The data passed to the callback
 * @param object The object for which the 'clicked' event was triggered(not used here)
 * @param event_info Additional event information
 */
static void
_btn_reset_cb(void *data, Evas_Object * object, void *event_info)
{
  elm_entry_entry_set(GLOBAL_DEBUG_BOX, "");

  _image_util_reset_cb(data, object, event_info);
}

/**
 * @brief Creates a basic display.
 *
 * @param name The display subtitle
 * @param callback The callback function that should be invoked when
 *                 the display is popped
 * @return The display object
 */
Evas_Object *
_create_new_cd_display(char *name, void *callback)
{
  /* Create a scroller */
  Evas_Object *scroller = elm_scroller_add(s_info.win);
  evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);

  /* Create a new item */
  Elm_Object_Item *item =
      elm_naviframe_item_push(s_info.navi, "Machine Learning", NULL, NULL,
      scroller, NULL);
  elm_object_item_part_text_set(item, "subtitle", name);

  if (callback != NULL)
    elm_naviframe_item_pop_cb_set(item, (Elm_Naviframe_Item_Pop_Cb) callback,
        NULL);
  else
    elm_naviframe_item_pop_cb_set(item, _pop_cb, NULL);

  /* Create the main box */
  Evas_Object *box = elm_box_add(scroller);
  elm_object_content_set(scroller, box);
  elm_box_horizontal_set(box, EINA_FALSE);
  evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  evas_object_show(box);

  /* Create a box for adding content */
  Evas_Object *bbox = elm_box_add(box);
  Evas_Coord padding_between_buttons = 3;
  elm_box_padding_set(bbox, 0, padding_between_buttons);
  elm_box_horizontal_set(bbox, EINA_FALSE);
  evas_object_size_hint_align_set(bbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(bbox, EVAS_HINT_EXPAND, 0.0);
  evas_object_show(bbox);

  /* Create the "Reset" button */
  Evas_Object *bt = elm_button_add(box);
  elm_object_text_set(bt, "Reset");
  evas_object_size_hint_align_set(bt, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(bt, EVAS_HINT_EXPAND, 0.0);
  evas_object_smart_callback_add(bt, "clicked", _btn_reset_cb, callback);
  evas_object_show(bt);

  /* Create a message box */
  Evas_Object *display_window = elm_entry_add(box);
  evas_object_size_hint_align_set(display_window, EVAS_HINT_FILL,
      EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(display_window, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  evas_object_show(display_window);

  elm_entry_editable_set(display_window, EINA_FALSE);
  elm_entry_scrollable_set(display_window, EINA_TRUE);

  GLOBAL_DEBUG_BOX = display_window;
  elm_scroller_policy_set(GLOBAL_DEBUG_BOX, ELM_SCROLLER_POLICY_OFF,
      ELM_SCROLLER_POLICY_ON);

  elm_box_pack_end(box, bbox);
  elm_box_pack_end(box, display_window);
  elm_box_pack_end(box, bt);

  return bbox;
}

/**
 * @brief Creates essential objects: window, conformant and layout.
 *
 * @param user_data The data passed from the callback registration function(not used here)
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise
 */
Eina_Bool
view_create(void *user_data)
{
  /* Create the window */
  s_info.win = view_create_win(PACKAGE);
  if (s_info.win == NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a window.");
    return EINA_FALSE;
  }

  /* Create the conformant */
  s_info.conform = view_create_conformant_without_indicator(s_info.win);
  if (s_info.conform == NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a conformant");
    return EINA_FALSE;
  }

  /* Create the naviframe */
  s_info.navi = view_create_naviframe(s_info.conform);
  if (s_info.navi == NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "failed to create a naviframe");
    return EINA_FALSE;
  }

  create_buttons_in_main_window();

  /* Show the window after main view is set up */
  evas_object_show(s_info.win);
  return EINA_TRUE;
}

/**
 * @brief Creates a basic window named package.
 *
 * @param pkg_name Name of the window
 * @return The newly created window
 */
Evas_Object *
view_create_win(const char *pkg_name)
{
  Evas_Object *win = NULL;

  /**
	 * Window
	 * Create and initialize elm_win.
	 * elm_win is mandatory to manipulate the window.
	 */
  win = elm_win_util_standard_add(pkg_name, pkg_name);
  elm_win_conformant_set(win, EINA_TRUE);
  elm_win_autodel_set(win, EINA_TRUE);
  elm_win_indicator_mode_set(win, ELM_WIN_INDICATOR_SHOW);
  elm_win_indicator_opacity_set(win, ELM_WIN_INDICATOR_OPAQUE);

  /* Rotation setting */
  if (elm_win_wm_rotation_supported_get(win)) {
    int rots[4] = { 0, 90, 180, 270 };
    elm_win_wm_rotation_available_rotations_set(win, (const int *)(&rots), 4);
  }

  return win;
}

/**
 * @brief Creates a conformant without indicator for wearable app.
 * @details Conformant is mandatory for base GUI to have proper size.
 *
 * @param win The object to which you want to add this conformant
 * @return The newly created conformant
 */
Evas_Object *
view_create_conformant_without_indicator(Evas_Object * win)
{
  /**
	 * Conformant
	 * Create and initialize elm_conformant.
	 * elm_conformant is mandatory for base GUI to have proper size
	 * when indicator or virtual keypad is visible.
	 */
  Evas_Object *conform = NULL;

  if (win == NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "window is NULL.");
    return NULL;
  }

  conform = elm_conformant_add(win);
  evas_object_size_hint_align_set(conform, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(conform, EVAS_HINT_EXPAND,
      EVAS_HINT_EXPAND);
  elm_win_resize_object_add(win, conform);

  evas_object_show(conform);

  return conform;
}

/**
 * @brief Creates a naviframe.
 *
 * @param win The object to which you want to add this naviframe
 * @return The newly created naviframe
 */
Evas_Object *
view_create_naviframe(Evas_Object * conform)
{
  /**
   * Naviframe
   * Create and initialize elm_naviframe.
   */
  Evas_Object *navi = NULL;
  if (conform == NULL) {
    dlog_print(DLOG_ERROR, LOG_TAG, "window is NULL.");
    return NULL;
  }

  navi = elm_naviframe_add(conform);
  evas_object_size_hint_align_set(navi, EVAS_HINT_FILL, EVAS_HINT_FILL);
  evas_object_size_hint_weight_set(navi, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
  eext_object_event_callback_add(navi, EEXT_CALLBACK_BACK,
      eext_naviframe_back_cb, NULL);
  elm_object_content_set(conform, navi);

  evas_object_show(navi);

  return navi;
}
