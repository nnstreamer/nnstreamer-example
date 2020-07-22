/**
 * @file OrientationDetection.cs
 * @date 22 JUL 2020
 * @brief Manage the Orientation Detection app
 * @see  https://github.com/nnstreamer/nnstreamer-example.git
 * @author Yongjoo Ahn <yongjoo1.ahn@samsung.com>
 * @bug No known bugs except for NYI items
 */

using System;
using Xamarin.Forms;
using System.Threading.Tasks;
using Tizen.MachineLearning.Inference;
using Log = Tizen.Log;

namespace OrientationDetection
{
  /**
   * @brief Orientation Detection App
   */
  public class App : Application
  {
    private const string TAG = "OrientationDetection";
    private Label label;
    private static string ResourcePath = Tizen.Applications.Application.Current.DirectoryInfo.Resource;
    private int orientation = 12;
    private Pipeline pipeline_handle;
    private Pipeline.SinkNode sink_handle;

    /**
     * @brief Initialize the nnstreamer pipline and manage the handles
     */
    private void init_pipeline()
    {
      string model_path = ResourcePath + "orientation_detection.tflite";
      string pipeline_description =
          "tensor_src_tizensensor type=accelerometer framerate=10/1 ! " +
          "tensor_filter framework=tensorflow-lite model=" + model_path + " ! " +
          "tensor_sink name=sinkx sync=true";

      pipeline_handle = new Pipeline(pipeline_description);
      if (pipeline_handle == null)
      {
        Log.Error(TAG, "Cannot create pipline");
        return;
      }

      sink_handle = pipeline_handle.GetSink("sinkx");
      if (sink_handle == null)
      {
        Log.Error(TAG, "Cannot get sink handle");
        return;
      }

      sink_handle.DataReceived += SinkxDataReceived;
    }

    /**
     * @brief Destroy the pipeline
     */
    private void destroy_pipeline()
    {
      sink_handle.DataReceived -= SinkxDataReceived;
      pipeline_handle.Dispose();
    }

    /**
     * @brief get output from the tensor_sink and update the orientation
     */
    private void SinkxDataReceived(object sender, DataReceivedEventArgs args)
    {
      TensorsData data_from_sink = args.Data;
      if (data_from_sink == null)
      {
        Log.Error(TAG, "Cannot get TensorsData");
        return;
      }

      byte[] out_buffer;
      out_buffer = data_from_sink.GetTensorData(0);
      float[] output = new float[4];
      Buffer.BlockCopy(out_buffer, 0, output, 0, out_buffer.Length);

      orientation = 0;

      for (int i = 1; i < 4; ++i)
        if (output[orientation] < output[i])
            orientation = i;

      orientation *= 3;
      if (orientation == 0)
          orientation = 12;
    }

    /**
     * @brief Update the label every 50ms
     */
    private async void LabelUpdateLoop()
    {
      while (true)
      {
        await Task.Delay(50);
        label.Text = string.Format("{0}", orientation);
      }
    }

    /**
     * @brief Orientation Detection App Class
     */
    public App()
    {
      label = new Label
      {
        Text = "12",
        HorizontalOptions = LayoutOptions.Center,
        FontSize = 24,
      };

      MainPage = new ContentPage
      {
        Content = new StackLayout
        {
          VerticalOptions = LayoutOptions.Center,
          Children = {
            label,
            new Label
            {
              HorizontalTextAlignment = TextAlignment.Center,
              Text = "UP",
              FontSize = 24,
            },
          }
        }
      };
    }

    /**
     * @brief Handle when app starts
     */
    protected override void OnStart()
    {
      LabelUpdateLoop();
    }
    
    /**
     * @brief Handle when app sleeps
     */
    protected override void OnSleep()
    {
      pipeline_handle.Stop();
      destroy_pipeline();
    }

    /**
     * @brief Handle when app resumes
     */
    protected override void OnResume()
    {
      init_pipeline();
      pipeline_handle.Start();
    }
  }
}
