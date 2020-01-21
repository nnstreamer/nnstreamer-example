/**
 * @file TextClassification.xaml.cs
 * @date 13 JAN 2020
 * @brief Manage the app
 * @see  https://github.com/nnsuite/nnstreamer-example.git
 * @author Gichan Jang <gichan2.jang@samsung.com>
 * @bug No known bugs except for NYI items
 *
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Xamarin.Forms;
using TextClassification.Views;

namespace TextClassification
{
  /**
  * @brief Text Classification Application Class
  */
  public partial class App : Application
  {
    /**
    * @brief Initializes a new instance of the App class.
    */
    public App()
    {
      InitializeComponent();

      /* Load main page. */
      MainPage = new NavigationPage(new UserInput());
    }
    /**
    * @brief On start method
    */
    protected override void OnStart()
    {
      /* Handle when your app starts */
    }

    /**
    * @brief On sleep method
    */
    protected override void OnSleep()
    {
      /* Handle when your app sleeps */
    }
    /**
    * @brief On resume method
    */
    protected override void OnResume()
    {
      /* Handle when your app resumes */
    }
  }
}
