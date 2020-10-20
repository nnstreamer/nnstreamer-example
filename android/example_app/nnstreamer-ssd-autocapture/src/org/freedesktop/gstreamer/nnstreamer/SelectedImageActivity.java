package org.freedesktop.gstreamer.nnstreamer;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;

public class SelectedImageActivity extends Activity implements View.OnClickListener {
    private boolean initialized = false;

    private ImageView imageView_preview;

    private Button buttonOK;

    private Bitmap photo;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_selected_image);

        initActivity();
    }

    private void initActivity() {
        if (initialized) {
            return;
        }

        /* set captured photo to ImageView */
        Intent intent = getIntent();

        byte[] arr = intent.getByteArrayExtra("photo");

        photo = BitmapFactory.decodeByteArray(arr,0, arr.length);

        imageView_preview = (ImageView) this.findViewById(R.id.selectedImage_imageview);
        imageView_preview.setImageBitmap(photo);

        buttonOK = (Button) this.findViewById(R.id.selectedImage_button_OK);
        buttonOK.setOnClickListener(this);

        initialized = true;
    }

    @Override
    public void onClick(View v) {
        final int viewId = v.getId();

        switch (viewId) {
            case R.id.selectedImage_button_OK:
                finish();
                break;
            default:
                break;
        }
    }
}
