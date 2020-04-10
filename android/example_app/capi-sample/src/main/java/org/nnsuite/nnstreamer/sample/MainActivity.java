package org.nnsuite.nnstreamer.sample;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;

public class MainActivity extends Activity {
    private static final String TAG = "NNStreamer-Sample";

    private static final int PERMISSION_REQUEST_CODE = 3;
    private static final String[] requiredPermissions = new String[] {
            Manifest.permission.READ_EXTERNAL_STORAGE,
            Manifest.permission.WRITE_EXTERNAL_STORAGE
    };

    private native boolean nativeRunSample(Context context);

    /* Load nnstreamer-sample library on application startup. */
    static {
        System.loadLibrary("nnstreamer-sample");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        /* Check permissions. */
        for (String permission : requiredPermissions) {
            if (!checkPermission(permission)) {
                ActivityCompat.requestPermissions(this,
                        requiredPermissions, PERMISSION_REQUEST_CODE);
                return;
            }
        }

        runSample();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                @NonNull String permissions[], @NonNull int[] grantResults) {
        if (requestCode == PERMISSION_REQUEST_CODE) {
            for (int grant : grantResults) {
                if (grant != PackageManager.PERMISSION_GRANTED) {
                    Log.i(TAG, "Permission denied, close app.");
                    finish();
                    return;
                }
            }

            runSample();
            return;
        }

        finish();
    }

    /**
     * Check the permission is granted.
     */
    private boolean checkPermission(final String permission) {
        return (ContextCompat.checkSelfPermission(this, permission)
                == PackageManager.PERMISSION_GRANTED);
    }

    /**
     * Call native to run sample pipeline.
     */
    private void runSample() {
        if (!nativeRunSample(this)) {
            Log.e(TAG, "Failed to call native sample.");
        }
    }
}
