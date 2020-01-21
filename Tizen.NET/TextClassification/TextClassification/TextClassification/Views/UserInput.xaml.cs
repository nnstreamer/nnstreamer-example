/**
 * @file UserInput.xaml.cs
 * @date 13 JAN 2020
 * @brief Main page to get user input
 * @see  https://github.com/nnsuite/nnstreamer-example.git
 * @author Gichan Jang <gichan2.jang@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

using System;
using Xamarin.Forms;
using Xamarin.Forms.Xaml;
using System.Threading;
using Log = Tizen.Log;


/**
* @brief Show User input page
*/
namespace TextClassification.Views
{
  /**
  * @brief Content page of User input
  */
  [XamlCompilation(XamlCompilationOptions.Compile)]
  public partial class UserInput : ContentPage
  {
    const string TAG = "TextClassification.Views.UserInput";
    private string Comment;
    private HandleText TextModel = new HandleText();

    /**
    * @brief Initializes a new instance of the class.
    */
    public UserInput()
    {
      InitializeComponent();

      var entryComment = new Entry();

      CommentChange(entryComment);

    }

    /**
    * @brief Event when "Get result" button is clicked.
    */
    public async void CreateClicked(object sender, EventArgs args)
    {
      Button button = (Button)sender;
      InputLabel.Text = Comment;
      float[] input;
      float[] output = new float[2];

      /* Initialize the model */
      TextModel.init_model();
      /* Convert user input to float array */
      input = TextModel.SplitStr(Comment);

      if (string.IsNullOrEmpty(Comment))
      {
        Log.Warn(TAG, "User input is empty");
        /* Check wrong Input parameter */
        await DisplayAlert("Error!", "Please enter your comment", "OK");
      }
      else if (Equals(button.Text, "Single-shot"))
      {
        /*Invoke the model */
        API.Text = "Single-shot";
        output = TextModel.invoke_mode(input);
      }
      else if (Equals(button.Text, "Pipeline"))
      {
        Log.Warn(TAG, "Unsupported API");
        await DisplayAlert("Error!", "Pipeline API will be supported soon.", "OK");
      }
      else
      {
        Log.Error(TAG, "Unknown button is pressed");
      }

      /* Show text classification resut */
      if (output[0].Equals(0) && output[1].Equals(0))
      {
        Out_neg.Text = "Invalid result";
        Out_pos.Text = "InValid result";
      }
      else
      {
        Out_neg.Text = output[0].ToString();
        Out_pos.Text = output[1].ToString();
      }
    }
    /**
    * @brief Event when H/W back button is clicked.
    */
    protected override bool OnBackButtonPressed()
    {
      System.Environment.Exit(1);
      return true;
    }

    /**
    * @brief Entry object get comment from user input
    */
    private void CommentChange(Entry entryComment) => entryComment.TextChanged += UserComment;

    /**
    * @brief Set user comment received from user input event
    * @detail <param name="sender"> Parameter about which object is invoked the current event. </param>
    *         <param name="e"> Event data of comment changed </param>
    */
    private void UserComment(object sender, TextChangedEventArgs e) => Comment = e.NewTextValue;
  }
}
