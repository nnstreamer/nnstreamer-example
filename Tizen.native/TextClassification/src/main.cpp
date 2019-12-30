/**
 * @file main.cpp
 * @date 28 December 2019
 * @brief TIZEN Native Example App of Text Classification with NNStreamer/C-API.
 * @see  https://github.com/nnsuite/nnstreamer
 * @author Gichan Jang <gichan2.jang@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

#include <dali-toolkit/dali-toolkit.h>
#include <app_common.h>
#include <glib.h>
#include <glib/gstdio.h>        /* GStatBuf */
#include <nnstreamer.h>
#include <iostream>
#include <string>
#include <dlog.h>

using namespace Dali;
using namespace Dali::Toolkit;

gfloat TextClassificationOutput[2];
#define MAX_SENTENCE_LENGTH 256
#ifdef  LOG_TAG
#undef  LOG_TAG
#endif
#define LOG_TAG "NNStreamer_Text_Classification"
/**
 * @brief Window view structure
 */
class WindowView: public ConnectionTracker
{
public :
	WindowView (Application & application)
    :
    mApplication (application),
    mStageHeight (0.0f),
    mStageWidth (0.0f)
    {
      /* Connect to the Application's init signal */
      mApplication.InitSignal ().Connect (this, &WindowView::ViewCreate);
    }
    
  /**
  * @brief Initialize the view and application
  */  void
  ViewCreate (Application & application)
  {
    /* Get a handle to the stage */
    Stage stage = Stage::GetCurrent ();

    /* Set Stage Background color to White */
    stage.SetBackgroundColor (Color::WHITE);

    /* Get stage height and width to define other actor's size */
    mStageHeight = stage.GetSize ().height;

    /* Get stage width and width to define other actor's size */
    mStageWidth = stage.GetSize ().width;

    /* Make <Get String>'s Title TextLabel by using CreateTitleLable() */
    TextLabel getStringLabel = CreateLabel ("Text Classification");
    getStringLabel.SetSize (mStageWidth, mStageHeight * 0.066);
    stage.Add (getStringLabel);

    /* Make TextLabel for Name TextField by using CreateTitleLable() */
    TextLabel nameFieldLabel = CreateLabel ("Comment :");
    nameFieldLabel.SetSize (mStageWidth * 0.3, mStageHeight * 0.066);
    nameFieldLabel.SetPosition (mStageWidth * 0.1, mStageHeight * 0.112);
    stage.Add (nameFieldLabel);

    /* Make Name TextField to get Name */
    mNameField = TextField::New ();
    mNameField.SetAnchorPoint (AnchorPoint::TOP_LEFT);
    mNameField.SetParentOrigin (ParentOrigin::TOP_LEFT);
    mNameField.SetSize (mStageWidth * 0.30, mStageHeight * 0.066);
    mNameField.SetPosition (mStageWidth * 0.4, mStageHeight * 0.102);
    mNameField.SetProperty (TextField::Property::VERTICAL_ALIGNMENT, "CENTER");
    stage.Add (mNameField);

    /* Make PushButton to get Text from Name TextField */
    PushButton nameFieldButton = PushButton::New ();
    nameFieldButton.SetAnchorPoint (AnchorPoint::TOP_LEFT);
    nameFieldButton.SetParentOrigin (ParentOrigin::TOP_LEFT);
    nameFieldButton.SetSize (mStageWidth * 0.2, mStageHeight * 0.05);
    nameFieldButton.SetPosition (mStageWidth * 0.7, mStageHeight * 0.112);

    /* Connect click signal to button */
    nameFieldButton.ClickedSignal ().Connect (this, &WindowView::NameFieldButtonClicked);
    stage.Add (nameFieldButton);

    /* Make TextLabel to use custom inner text for PushButton */
    TextLabel nameFieldButtonText = CreateLabel ("Enter");
    nameFieldButtonText.SetSize (mStageWidth * 0.2, mStageHeight * 0.066);
    nameFieldButtonText.SetProperty (TextLabel::Property::TEXT_COLOR, Color::WHITE);

    /* Add inner text to PushButton */
    nameFieldButton.Add (nameFieldButtonText);

    /* Display the user input */
    mNameFieldResult = CreateLabel ("User comment is\n ----------- ");
    mNameFieldResult.SetSize (mStageWidth, mStageHeight * 0.132);
    mNameFieldResult.SetPosition (0, mStageHeight * 0.198);
    stage.Add (mNameFieldResult);

    /* Display the result of text classification */
    textClassificationResult =
        CreateLabel ("Output\nNegative : ----------\nPositive : ----------");
    textClassificationResult.SetSize (mStageWidth, mStageHeight * 0.132);
    textClassificationResult.SetPosition (0, mStageHeight * 0.398);
    stage.Add (textClassificationResult);

    /* Connect to key event signals */
    stage.KeyEventSignal ().Connect (this, &WindowView::OnKeyEvent);

    init_pipeline ();
  }

/**
* @brief Create TextLabel for displaying results and notice section's theme
*/
  TextLabel
  CreateLabel (const std::string text)
  {
    /* Make TextLabel Object and set inner text */
    TextLabel labelObj = TextLabel::New (text);
    /* Set basic properties */
    labelObj.SetAnchorPoint (AnchorPoint::TOP_LEFT);
    labelObj.SetParentOrigin (ParentOrigin::TOP_LEFT);
    labelObj.SetProperty (TextLabel::Property::VERTICAL_ALIGNMENT, "CENTER");
    labelObj.SetProperty (TextLabel::Property::HORIZONTAL_ALIGNMENT, "CENTER");

    /* Make TextLabel show multiline text */
    labelObj.SetProperty (TextLabel::Property::MULTI_LINE, true);
    labelObj.SetProperty (TextLabel::Property::VERTICAL_ALIGNMENT, "TOP");

    return labelObj;
  }

  /**
  * @brief Get input String via this callback function
  */
  bool
  NameFieldButtonClicked (Button button)
  {
    dlog_print (DLOG_DEBUG, LOG_TAG, "Get input string from the user");

    gchar *result;

    /* Get input String as Property::Value */
    Property::Value fieldText =
        mNameField.GetProperty (TextField::Property::TEXT);

    /* Convert Property::Value to std::string to use in TextLabel */
    std::string fieldTextString = fieldText.Get < std::string > ();
    mNameField.SetProperty (TextLabel::Property::TEXT, "");

    /* Reset mNameFieldResult's text */
    mNameFieldResult.SetProperty (TextLabel::Property::TEXT,
        "Your comment is\n" + fieldTextString);

    /* Convert uppercase to lowercase to match with dictionary */
    std::transform (fieldTextString.begin (), fieldTextString.end (),
        fieldTextString.begin (),::tolower);

    handle_input_string (fieldTextString);

    g_usleep (50000);

    result = g_strdup_printf ("Output\nNegative : %.6f\nPositive : %.6f",
        TextClassificationOutput[0], TextClassificationOutput[1]);
    textClassificationResult.SetProperty (TextLabel::Property::TEXT, result);

    g_free (result);

    return true;
  }

  /**
  * @brief Off the app when BACK H/W Key is clicked
  */
  void
  OnKeyEvent (const KeyEvent & event)
  {
    if (event.state == KeyEvent::Down) {
      if (IsKey (event, DALI_KEY_ESCAPE) || IsKey (event, DALI_KEY_BACK)) {
        dlog_print (DLOG_DEBUG, LOG_TAG, "BACK H/W key is clicked");
        ml_pipeline_sink_unregister (sink_handle);
        ml_pipeline_src_release_handle (src_handle);
        ml_pipeline_stop (handle);
        ml_pipeline_destroy (handle);
        g_free (float_array);
  	    g_strfreev (labels);
  	    g_hash_table_remove_all (words);
        ml_tensors_info_destroy (info);
        ml_tensors_data_destroy (input);

        mApplication.Quit ();
      }
    }
  }

  /**
  * @brief Initialize the pipeline
  */
  void
  init_pipeline ()
  {
    gchar *contents;
    gint ret;

    char *path = app_get_resource_path ();

    dlog_print (DLOG_DEBUG, LOG_TAG, "Initialize the pipeline");
    model = g_build_filename (path, "text_classification.tflite", NULL);
    gchar *label = g_build_filename (path, "labels.txt", NULL);
    gchar *vocab = g_build_filename (path, "vocab.txt", NULL);

    /* check the loaded files */
      if (!g_file_test (model, G_FILE_TEST_EXISTS) ||
          !g_file_test (label, G_FILE_TEST_EXISTS) ||
          !g_file_test (vocab, G_FILE_TEST_EXISTS)) {
          dlog_print (DLOG_ERROR, LOG_TAG, "Failed to load the model.");
          goto error;
      }

    /* load label file */
    if (g_file_get_contents (label, &contents, NULL, NULL)) {
      labels = g_strsplit (contents, "\n", -1);
      g_free (contents);
      dlog_print (DLOG_DEBUG, LOG_TAG, "Label file is sucessfully loaded");

    } else {
      dlog_print (DLOG_ERROR, LOG_TAG, "Failed to load label file.");
      goto error;
    }

    /* load dictionary */
    if (g_file_get_contents (vocab, &contents, NULL, NULL)) {
      gchar **dics = g_strsplit (contents, "\n", -1);
      guint dics_len = g_strv_length (dics);
      guint i;
      dlog_print (DLOG_DEBUG, LOG_TAG, "Vocab file is sucessfully loaded");

      /* init hash table */
      words = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      g_assert (words);

      for (i = 0; i < dics_len; i++) {
        /* get word and index */
        gchar **word = g_strsplit (dics[i], " ", 2);
        if (g_strv_length (word) == 2) {
          gint word_index = (gint) g_ascii_strtoll (word[1], NULL, 10);

          gpointer key = g_strdup (word[0]);
          gpointer value = GINT_TO_POINTER (word_index);

          g_hash_table_insert (words, key, value);
        } else {
          dlog_print (DLOG_ERROR, LOG_TAG, "Failed to parse voca.");
        }
        g_strfreev (word);
      }

      g_strfreev (dics);
      g_free (contents);
    } else {
      dlog_print (DLOG_ERROR, LOG_TAG, "Failed to load dictionary file.");
    }

    pipeline = g_strdup_printf
        ("appsrc name=appsrc ! other/tensor,dimension=(string)256:1:1:1,type=(string)float32,framerate=(fraction)0/1 ! "
        "tensor_filter framework=tensorflow-lite input=256:1:1:1 inputtype=float32 model=%s ! tensor_sink name=tensor_sink",
        model);

    ret = ml_pipeline_construct (pipeline, NULL, NULL, &handle);
    if(ret != ML_ERROR_NONE) {
        dlog_print (DLOG_WARN, LOG_TAG, "Failed to construct the pipleline.");
    }

    /* get tensor element using name */
    ret = ml_pipeline_src_get_handle (handle, "appsrc", &src_handle);
    if(ret != ML_ERROR_NONE) {
      dlog_print (DLOG_WARN, LOG_TAG, "Failed to get source handle.");
    }
    /* register call back function when new data is arrived on sink pad */
    ret = ml_pipeline_sink_register (handle, "tensor_sink", new_data_cb, NULL,
        &sink_handle);
    if(ret != ML_ERROR_NONE) {
      dlog_print (DLOG_WARN, LOG_TAG, "Failed to get sink handle.");
    }

    ret = ml_pipeline_start (handle);
    if(ret != ML_ERROR_NONE) {
      dlog_print (DLOG_WARN, LOG_TAG, "Failed to start the pipeline.");
    }

    g_usleep (500000);          /* wait for start */

    g_free (pipeline);
    g_free (path);
error:
    g_free (model);
    g_free (label);
    g_free (vocab);

  }

  /**
   * @brief Callback for tensor sink signal.
   */
  static void
  new_data_cb (const ml_tensors_data_h data, const ml_tensors_info_h info,
      void *user_data)
  {
    gfloat *data_ptr;
    size_t data_size;
    
    dlog_print (DLOG_DEBUG, LOG_TAG, "New data is arrived on the sink pad");

    gint ret = ml_tensors_data_get_tensor_data (data, 0, (void **) &data_ptr, &data_size);
    if(ret != ML_ERROR_NONE) {
      dlog_print (DLOG_WARN, LOG_TAG, "Failed to get tensor data.");
    }
    TextClassificationOutput[0] = data_ptr[0];
    TextClassificationOutput[1] = data_ptr[1];
  }

  /**
   * @brief Handle input string and push to source pad.
   */
  void
  handle_input_string (std::string str)
  {
    gchar **tokens;
    gchar sentence[MAX_SENTENCE_LENGTH + 1] = { 0, };
    gint tokens_len,i, value, start, unknown, pad, ret;

    str.copy (sentence, str.size () + 1);
    sentence[str.size ()] = '\0';

    float_array = (gfloat *) g_malloc0 (sizeof (gfloat) * MAX_SENTENCE_LENGTH);

    start = GPOINTER_TO_INT (g_hash_table_lookup (words, "<START>"));
    pad = GPOINTER_TO_INT (g_hash_table_lookup (words, "<PAD>"));
    unknown = GPOINTER_TO_INT (g_hash_table_lookup (words, "<UNKNOWN>"));

    float_array[0] = (gfloat) start;

    tokens = g_strsplit_set (sentence, " \n\t", MAX_SENTENCE_LENGTH);
    tokens_len = g_strv_length (tokens);

    for (i = 1; i <= tokens_len; i++) {
      value = GPOINTER_TO_INT (g_hash_table_lookup (words, tokens[i - 1]));
      float_array[i] = (gfloat) (value > 0 ? value : unknown);
    }

    while (i < MAX_SENTENCE_LENGTH) {
      float_array[i++] = (gfloat) pad;
    }

    in_dim[0] = 256;
    in_dim[1] = in_dim[2] = in_dim[3] = 1;
    ml_tensors_info_create (&info);
    ml_tensors_info_set_count (info, 1);
    ml_tensors_info_set_tensor_type (info, 0, ML_TENSOR_TYPE_FLOAT32);
    ml_tensors_info_set_tensor_dimension (info, 0, in_dim);

    ret = ml_tensors_data_create (info, &input);
    if(ret != ML_ERROR_NONE) {
      dlog_print (DLOG_WARN, LOG_TAG, "Failed to create data.");
    }
    ml_tensors_data_set_tensor_data (input, 0, float_array,
        sizeof (gfloat) * MAX_SENTENCE_LENGTH);
    ret = ml_pipeline_src_input_data (src_handle, input, ML_PIPELINE_BUF_POLICY_DO_NOT_FREE);
    if(ret != ML_ERROR_NONE) {
      dlog_print (DLOG_WARN, LOG_TAG, "Failed to push data on src pad.");
    }
  }

private:
  Application & mApplication;
  float mStageHeight;
  float mStageWidth;
  TextLabel mNameFieldResult;
  TextField mNameField;
  TextLabel textClassificationResult;

  gchar *pipeline;
  gchar *model;
  ml_pipeline_h handle;
  ml_pipeline_src_h src_handle;
  ml_pipeline_sink_h sink_handle;
  ml_tensors_data_h input;
  ml_tensor_dimension in_dim;
  ml_tensors_info_h info;
  gchar ** labels;
  GHashTable *words;
  gfloat *float_array;
};

/**
* @brief Entry point for Tizen applications
*/
int
main (int argc, char **argv)
{
  Application application = Application::New (&argc, &argv);
  WindowView test (application);
  application.MainLoop ();

  return 0;
}
