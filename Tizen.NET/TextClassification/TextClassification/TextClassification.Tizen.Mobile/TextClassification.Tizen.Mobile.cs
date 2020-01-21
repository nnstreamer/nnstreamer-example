/**
 * @file TextClassification.Tizen.Mobile.cs
 * @date 13 JAN 2020
 * @brief Text classification mobile app.
 * @see  https://github.com/nnsuite/nnstreamer-example.git
 * @author Gichan Jang <gichan2.jang@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

using System;
namespace TextClassification.Tizen.Mobile
{
  /**
  * @brief Represents xamarin forms of tizen platform app
  */
  class Program : global::Xamarin.Forms.Platform.Tizen.FormsApplication
  {
    /**
    * @brief On create method
    */
    protected override void OnCreate()
    {
      base.OnCreate();
      LoadApplication(new App());
    }
    /**
    * @brief Main method of text classification tizen mobile
    */
    static void Main(string[] args)
    {
      var app = new Program();

      /* Initiailize App */
      global::Xamarin.Forms.Forms.Init(app);

      app.Run(args);
    }
  }
}
