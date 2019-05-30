package org.freedesktop.gstreamer.nnstreamer;

import android.Manifest;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.View;
import android.widget.Button;

public class FileChooser extends Activity {
    public static final String EXTRA_MESSAGE = "org.freedesktop.gstreamer.nnstreamer.mediassd.MESSAGE";
    private static final String TAG = "NNStreamer-MediaSSD-FileChooser";
    private static final int REQUEST_CODE = 5515;
    private static final int PERMISSION_REQUEST_ALL = 3;
    private static Button button;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        /* Check permissions */
        if (!checkPermission(Manifest.permission.INTERNET) ||
            !checkPermission(Manifest.permission.READ_EXTERNAL_STORAGE) ||
            !checkPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE) ||
            !checkPermission(Manifest.permission.WAKE_LOCK)) {
            ActivityCompat.requestPermissions(this,
                new String[] {
                    Manifest.permission.CAMERA,
                    Manifest.permission.INTERNET,
                    Manifest.permission.READ_EXTERNAL_STORAGE,
                    Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    Manifest.permission.WAKE_LOCK
                }, PERMISSION_REQUEST_ALL);
            return;
        }

        makeChooseButton();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String permissions[], int[] grantResults) {
        if (requestCode == PERMISSION_REQUEST_ALL) {
            for (int grant : grantResults) {
                if (grant != PackageManager.PERMISSION_GRANTED) {
                    Log.i(TAG, "Permission denied, close app.");
                    finish();
                    return;
                }
            }

            makeChooseButton();
            return;
        }

        finish();
    }

    /**
     * Choose button to select a media file.
     */
    private void makeChooseButton() {
        button = new Button(this);
        button.setText("Choose a file to play");
        button.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showChooser();
            }
        });
        setContentView(button);
    }

    /**
     * Check the given permission is granted.
     */
    private boolean checkPermission(final String permission) {
        return (ContextCompat.checkSelfPermission(this, permission)
                == PackageManager.PERMISSION_GRANTED);
    }

    /**
     * Show Android's built-in file chooser
     */
    private void showChooser() {
        /* Use the GET_CONTENT intent from the utility class */
        Intent target = new Intent(Intent.ACTION_GET_CONTENT);
        target.setType("video/*");
        target.addCategory(Intent.CATEGORY_OPENABLE);

        /* Create the chooser Intent */
        Intent intent = Intent.createChooser(
                target, "Choose a file to play");
        try {
            startActivityForResult(intent, REQUEST_CODE);
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Activity not found");
        } catch (Exception e) {
            e.printStackTrace();
            throw e;
        }
    }

    /**
     * Convert a target uri to its absolute file name.
     */
    public static String getPath(final Context context, final Uri uri) {
        if (isMediaDocument(uri)) {
            final String docId = DocumentsContract.getDocumentId(uri);
            final String[] split = docId.split(":");
            final String type = split[0];

            Uri contentUri = null;
            if ("image".equals(type)) {
                contentUri = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
            } else if ("video".equals(type)) {
                contentUri = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
            } else if ("audio".equals(type)) {
                contentUri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
            }

            final String selection = "_id=?";
            final String[] selectionArgs = new String[] { split[1] };

            return getDataColumn(context, contentUri, selection,
                    selectionArgs);
        } else {
            final String docId = DocumentsContract.getDocumentId(uri);
            final String[] split = docId.split(":");
            return split[1];
        }
    }

    /**
     * Get the value of the data column for this Uri. This is useful for
     * MediaStore Uris, and other file-based ContentProviders.
     */
    public static String getDataColumn(Context context, Uri uri,
                                       String selection, String[] selectionArgs) {

        Cursor cursor = null;
        final String column = "_data";
        final String[] projection = { column };

        try {
            cursor = context.getContentResolver().query(uri, projection,
                    selection, selectionArgs, null);
            if (cursor != null && cursor.moveToFirst()) {
                final int index = cursor.getColumnIndexOrThrow(column);
                return cursor.getString(index);
            }
        } finally {
            if (cursor != null)
                cursor.close();
        }
        return null;
    }

    public static boolean isMediaDocument(Uri uri) {
        return "com.android.providers.media.documents".equals(uri
                .getAuthority());
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        switch (requestCode) {
            case REQUEST_CODE:
                /* If the file selection was successful */
                if (resultCode == RESULT_OK) {
                    if (data != null) {
                        /* Get the URI of the selected file */
                        final Uri uri = data.getData();
                        Log.i(TAG, "Uri = " + uri.toString());
                        try {
                            /* Get the file path from the URI */
                            String path = getPath(this, uri);
                            /* Start the actual activity! */
                            Intent intent = new Intent (this, NNStreamerActivity.class);
                            intent.putExtra(EXTRA_MESSAGE, path);
                            startActivity(intent);

                            finish();
                        } catch (Exception e) {
                            Log.e("FileSelectorTestActivity", "File select error", e);
                        }
                    }
                }
                break;
        }
        super.onActivityResult(requestCode, resultCode, data);
    }
}
