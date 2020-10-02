package org.freedesktop.gstreamer.nnstreamer;

import android.app.Activity;
<<<<<<< HEAD
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.Image;
import android.net.Uri;
import android.os.Environment;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;

public class PreviewActivity extends Activity implements View.OnClickListener {

    private boolean initialized = false;

    private ImageView imageView_preview;

    private Button buttonSave;
    private Button buttonRetry;

    private Bitmap photo;
=======
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;

public class PreviewActivity extends Activity {
>>>>>>> 99f11820237bd39e7e29c984c0e0c9e905c1b5c9

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_preview);
<<<<<<< HEAD

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

        imageView_preview = (ImageView) this.findViewById(R.id.preview_imageview);
        imageView_preview.setImageBitmap(photo);

        buttonSave = (Button) this.findViewById(R.id.preview_button_save);
        buttonSave.setOnClickListener(this);

        buttonRetry = (Button) this.findViewById(R.id.preview_button_retry);
        buttonRetry.setOnClickListener(this);

        initialized = true;
    }

    @Override
    public void onClick(View v) {
        final int viewId = v.getId();

        switch (viewId) {
            case R.id.preview_button_save:
                // Set file name with current time and save the bitmap
                Date date = new Date(System.currentTimeMillis());
                SimpleDateFormat sdfNow = new SimpleDateFormat("yyyyMMdd_HHmmss");
                String formatDate = sdfNow.format(date);
                saveBitmaptoJpeg(photo, "Autocapture", formatDate);
                finish();
                break;
            case R.id.preview_button_retry:
                finish();
                break;
            default:
                break;
        }
    }

    // Save bitmap to JPEG
    public void saveBitmaptoJpeg(Bitmap bitmap,String folder, String name){
        String ex_storage = Environment.getExternalStorageDirectory().getAbsolutePath();
        // Get Absolute Path in External Sdcard
        String folder_name = "/DCIM/"+folder+"/";
        String file_name = name+".jpg";
        String string_path = ex_storage+folder_name;


        File file_path;
        try{
            file_path = new File(string_path);
            if(!file_path.isDirectory()){
                file_path.mkdirs();
            }
            FileOutputStream out = new FileOutputStream(string_path+file_name);
            boolean flag = bitmap.compress(Bitmap.CompressFormat.JPEG, 100, out);
            out.flush();
            out.close();

            // Refresh to see this image in gallery
            Intent intent = new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE);
            File file = new File(string_path);
            intent.setData(Uri.fromFile(file));
            sendBroadcast(intent);

        }catch(FileNotFoundException exception){
            Log.e("FileNotFoundException", exception.getMessage());
        }catch(IOException exception){
            Log.e("IOException", exception.getMessage());
        }
=======
>>>>>>> 99f11820237bd39e7e29c984c0e0c9e905c1b5c9
    }
}