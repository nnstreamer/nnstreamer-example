package org.nnsuite.nnstreamer.sample;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

/**
 * Sample code to run the application with nnstreamer-api.
 * Before building this sample, copy nnstreamer-api library file (nnstreamer-YYYY-MM-DD.aar) into 'libs' directory.
 */
public class MainActivity extends Activity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

    }

    public void onTFLiteButtonClicked(View view) {
        Intent intent = new Intent(getApplicationContext(), TFLiteActivity.class);
        startActivity(intent);
    }

    public void onMXNetButtonClicked(View view) {
        Intent intent = new Intent(getApplicationContext(), MXNetActivity.class);
        startActivity(intent);
    }
}
