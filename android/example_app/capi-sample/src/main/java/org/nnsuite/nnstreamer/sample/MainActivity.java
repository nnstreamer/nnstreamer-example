package org.nnsuite.nnstreamer.sample;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;

import org.freedesktop.gstreamer.GStreamer;

public class MainActivity extends Activity {
    private static final String TAG = "NNStreamer-Sample";

    private native boolean nativeRunSample();

    /* Load nnstreamer-sample library on application startup. */
    static {
        System.loadLibrary("nnstreamer-sample");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        /* You SHOULD initialize GStreamer first. */
        try {
            GStreamer.init(getApplicationContext());
        } catch (Exception e) {
            Log.e(TAG, "Failed to initialize GStreamer.");
            finish();
            return;
        }

        setContentView(R.layout.activity_main);

        /* Call native to run sample pipeline. */
        if (!nativeRunSample()) {
            Log.e(TAG, "Failed to call native sample.");
        }
    }
}
