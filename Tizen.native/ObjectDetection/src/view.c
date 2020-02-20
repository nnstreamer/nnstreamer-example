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

#include "main.h"
#include "view.h"
#include "data.h"

//Evas_Object *GLOBAL_DEBUG_BOX;

/**
 * @brief Adds the text to the log entry.
 *
 * @param text  The text to be appended
 */
//void _add_entry_text(const char *text)
//{
//    Evas_Coord c_y;
//
//    elm_entry_entry_append(GLOBAL_DEBUG_BOX, text);
//    elm_entry_entry_append(GLOBAL_DEBUG_BOX, "<br>");
//    elm_entry_cursor_end_set(GLOBAL_DEBUG_BOX);
//    elm_entry_cursor_geometry_get(GLOBAL_DEBUG_BOX, NULL, &c_y, NULL, NULL);
//    elm_scroller_region_show(GLOBAL_DEBUG_BOX, 0, c_y, 0, 0);
//}

static struct view_info {
    Evas_Object *win;
    Evas_Object *conform;
    Evas_Object *navi;
} s_info = {
    .win = NULL,
    .conform = NULL,
    .navi = NULL,
};

/**
 * @brief Called when the main window of the application is closed.
 * @details It minimizes the applications preventing it from closing.
 * @remarks This function matches the Elm_Naviframe_Item_Pop_Cb() signature
 *          defined in the elm_naviframe_item.eo.legacy.h header file.
 *
 * @param data  The user data passed via void pointer (not used here)
 * @param item  The object that is going to be popped. This argument is not
 *              used in this case.
 *
 * @return @c EINA_TRUE to proceed with popping the object, otherwise @c
 *         EINA_FALSE to cancel the item popping process.
 */
//static Eina_Bool _pop_cb(void *data, Elm_Object_Item *item)
//{
//    elm_win_lower(s_info.win);
//    return EINA_FALSE;
//}

/**
 * @brief Creates a new button.
 *
 * @param display   The parent object for the newly created button
 * @param name      The text that will be displayed on the button
 * @param callback  The callback function that will be invoked when the button
 *                  is clicked.
 *
 * @return The newly created button object
 */
Evas_Object *_new_button(Evas_Object *display, char *name, void *callback)
{
    Evas_Object *bt = elm_button_add(display);
    elm_object_text_set(bt, name);
    evas_object_smart_callback_add(bt, "clicked", (Evas_Smart_Cb) callback, NULL);
    evas_object_size_hint_weight_set(bt, EVAS_HINT_EXPAND, 0.0);
    evas_object_size_hint_align_set(bt, EVAS_HINT_FILL, EVAS_HINT_FILL);
    elm_box_pack_end(display, bt);
    evas_object_show(bt);
    return bt;
}

/**
 * @brief Clears the debug display.
 * @details Called when the "Clear" button is clicked.
 * @remarks This function matches the Evas_Smart_Cb() signature defined in the
 *          Evas_Legacy.h header file.
 *
 * @param data        The user data passed via void pointer. This argument is not
 *                    used in this case.
 * @param object      A handle to the object on which the event occurred. In this
 *                    case it's a pointer to the button object. This argument is not
 *                    used in this case.
 * @param event_info  A pointer to a data which is totally dependent on the smart
 *                    object's implementation and semantic for the given event. This
 *                    argument is not used in this case.
 */
//static void _btn_clear_cb(void *data, Evas_Object *object, void *event_info)
//{
//    elm_entry_entry_set(GLOBAL_DEBUG_BOX, "");
//}

/**
 * @brief Creates a new view.
 *
 * @param name      The subtitle for the newly created view
 * @param callback  The callback function invoked when the view is being popped
 *
 * @return A box object, which is an element of the newly created view. All new
 *         UI elements of this view are held in this container.
 */
Evas_Object *_create_new_cd_display(char *name, void *callback)
{
    /* Create a scroller */
//    Evas_Object *scroller = elm_scroller_add(s_info.win);
//    evas_object_size_hint_weight_set(scroller, EVAS_HINT_EXPAND,
//            EVAS_HINT_EXPAND);

    /* Create a new item */
//    Elm_Object_Item *item = elm_naviframe_item_push(s_info.navi, "Multimedia",
//            NULL, NULL, scroller, NULL);
//    elm_object_item_part_text_set(item, "subtitle", name);

//    if (callback != NULL)
//        elm_naviframe_item_pop_cb_set(item, (Elm_Naviframe_Item_Pop_Cb) callback,
//                NULL);
//    else
//        elm_naviframe_item_pop_cb_set(item, _pop_cb, NULL);

    /* Create main box */
//    Evas_Object *box = elm_box_add(scroller);
//    elm_object_content_set(scroller, box);
    Evas_Object *box = elm_box_add(s_info.win);
    elm_object_content_set(s_info.win, box);
    elm_box_horizontal_set(box, EINA_FALSE);
    evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
    evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
    evas_object_show(box);

    return box;

    /* Create a box for adding content */
    //    Evas_Coord padding_between_buttons = 3;
    //    elm_box_padding_set(bbox, 0, padding_between_buttons);
//    Evas_Object *bbox = elm_box_add(box);
//    elm_box_horizontal_set(bbox, EINA_FALSE);
//    evas_object_size_hint_align_set(bbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
//    evas_object_size_hint_weight_set(bbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
//    elm_box_pack_end(box, bbox);
//    evas_object_show(bbox);

    /* Create a box for entry */
//    Evas_Object *ebox = elm_box_add(box);
//    elm_box_horizontal_set(ebox, EINA_FALSE);
//    evas_object_size_hint_align_set(ebox, EVAS_HINT_FILL, EVAS_HINT_FILL);
//    evas_object_size_hint_weight_set(ebox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
//    elm_box_pack_end(box, ebox);
//    evas_object_show(ebox);

    /* Create a message box */
//    GLOBAL_DEBUG_BOX = elm_entry_add(ebox);
//    elm_entry_editable_set(GLOBAL_DEBUG_BOX, EINA_FALSE);
//    elm_entry_scrollable_set(GLOBAL_DEBUG_BOX, EINA_TRUE);
//    elm_scroller_policy_set(GLOBAL_DEBUG_BOX, ELM_SCROLLER_POLICY_OFF,
//            ELM_SCROLLER_POLICY_ON);
//    evas_object_size_hint_align_set(GLOBAL_DEBUG_BOX, EVAS_HINT_FILL,
//            EVAS_HINT_FILL);
//    evas_object_size_hint_weight_set(GLOBAL_DEBUG_BOX, EVAS_HINT_EXPAND,
//            EVAS_HINT_EXPAND);
//    elm_box_pack_end(ebox, GLOBAL_DEBUG_BOX);
//    evas_object_show(GLOBAL_DEBUG_BOX);

    /* Create the "Clear" button */
//    _new_button(box, "Clear", _btn_clear_cb);

//    return bbox;
}

/**
 * @brief Creates essential objects: window, conformant and layout.
 *
 * @param user_data The data passed from the callback registration function (not used here)
 * @return @c EINA_TRUE on success, @c EINA_FALSE otherwise
 */
Eina_Bool view_create(void *user_data)
{
    /* Required to display the camera preview correctly */
    elm_config_accel_preference_set("3d");

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
Evas_Object *view_create_win(const char *pkg_name)
{
    Evas_Object *win = NULL;

    /*
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

    evas_object_smart_callback_add(win, "delete,request", NULL, NULL);

    return win;
}

/**
 * @brief Creates a layout to target parent object with edje file
 *
 * @param parent The object to which you want to add this layout
 * @param file_path File path of EDJ file will be used
 * @param group_name Name of group in EDJ you want to set to
 * @param cb_function The function will be called when back event is detected
 * @param user_data The user data to be passed to the callback functions
 * @return The newly created layout
 */
Evas_Object *view_create_layout(Evas_Object *parent, const char *file_path, const char *group_name, Eext_Event_Cb cb_function, void *user_data)
{
    Evas_Object *layout = NULL;

    if (parent == NULL) {
        dlog_print(DLOG_ERROR, LOG_TAG, "parent is NULL.");
        return NULL;
    }

    /* Create layout using EDC(an edje file) */
    layout = elm_layout_add(parent);
    elm_layout_file_set(layout, file_path, group_name);

    /* Layout size setting */
    evas_object_size_hint_weight_set(layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

    if (cb_function)
        eext_object_event_callback_add(layout, EEXT_CALLBACK_BACK, cb_function, user_data);

    evas_object_show(layout);

    return layout;
}

/**
 * @brief Creates a conformant without indicator for wearable app.
 * @details Conformant is mandatory for base GUI to have proper size.
 *
 * @param win The object to which you want to add this conformant
 * @return The newly created conformant
 */
Evas_Object *view_create_conformant_without_indicator(Evas_Object *win)
{
    /*
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
    evas_object_size_hint_weight_set(conform, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
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
Evas_Object *view_create_naviframe(Evas_Object *conform)
{
    /*
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
    eext_object_event_callback_add(navi, EEXT_CALLBACK_BACK, eext_naviframe_back_cb, NULL);
    elm_object_content_set(conform, navi);

    evas_object_show(navi);

    return navi;
}

/**
 * @brief Destroys window and frees its resources.
 */
void view_destroy(void)
{
    if (s_info.win == NULL)
        return;

    evas_object_del(s_info.win);
}

/**
 * @brief Destroys given layout.
 *
 * @param layout The layout to destroy
 */
void view_destroy_layout(Evas_Object *layout)
{
    evas_object_del(layout);
}
