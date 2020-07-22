/**
 * @file OrientationDetection.Tizen.Wearable.cs
 * @date 22 JUL 2020
 * @brief Orientation Detection wearable app.
 * @see  https://github.com/nnstreamer/nnstreamer-example.git
 * @author Yongjoo Ahn <yongjoo1.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

using Xamarin.Forms;

namespace OrientationDetection
{
  /**
   * @brief Represents xamarin forms of tizen platform app
   */
  class Program : Xamarin.Forms.Platform.Tizen.FormsApplication
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
     * @breif Main method of Orientation Detection for tizen wearable
     */
    static void Main(string[] args)
    {
      var app = new Program();
      Forms.Init(app);
      app.Run(args);
    }
  }
}
