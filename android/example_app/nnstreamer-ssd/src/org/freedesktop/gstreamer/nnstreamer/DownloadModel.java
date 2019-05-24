package org.freedesktop.gstreamer.nnstreamer;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.PowerManager;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;

public class DownloadModel extends AsyncTask<ArrayList<String>, String, Boolean> {
    private static final String TAG = "NNStreamer";

    private Activity activity;
    private String downloadPath;
    private PowerManager.WakeLock wakeLock;
    private ProgressDialog progressBar;
    private boolean isProgress;

    public DownloadModel(Activity a, String path) {
        activity = a;
        downloadPath = path;
        isProgress = true;
    }

    public boolean isProgress() {
        return isProgress;
    }

    @Override
    protected void onPreExecute() {
        super.onPreExecute();

        PowerManager pm = (PowerManager) activity.getSystemService(Context.POWER_SERVICE);

        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, getClass().getName());
        wakeLock.acquire();

        progressBar = new ProgressDialog(activity);

        progressBar.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
        progressBar.setMessage(activity.getText(R.string.download));
        progressBar.setIndeterminate(true);
        progressBar.setCancelable(false);
        progressBar.setCanceledOnTouchOutside(false);

        progressBar.show();
    }

    @Override
    protected Boolean doInBackground(ArrayList<String>... arrayLists) {
        final ArrayList<String> downloadList = arrayLists[0];
        boolean failed = false;

        File downloadDirectory = new File(downloadPath);
        if (!downloadDirectory.exists()) {
            downloadDirectory.mkdirs();
        }

        progressBar.setMax(downloadList.size());
        progressBar.setIndeterminate(false);

        for (int i = 0; i < downloadList.size(); i++) {
            String fileName = downloadList.get(i);

            publishProgress(fileName, Integer.toString(i));

            if (!downloadFile(fileName)) {
                failed = true;
                break;
            }
        }

        wakeLock.release();
        return !failed;
    }

    @Override
    protected void onProgressUpdate(String... progress) {
        super.onProgressUpdate(progress);

        progressBar.setMessage(activity.getText(R.string.download) + " " + progress[0]);
        progressBar.setProgress(Integer.parseInt(progress[1]));
    }

    @Override
    protected void onPostExecute(Boolean done) {
        super.onPostExecute(done);

        progressBar.dismiss();
        progressBar = null;

        if (done) {
            isProgress = false;
            activity.recreate();
        } else {
            showErrorDialog();
        }
    }

    private void showErrorDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(activity);

        builder.setCancelable(false);
        builder.setMessage(R.string.download_model_failed);

        builder.setNegativeButton(R.string.close, new DialogInterface.OnClickListener() {
            @Override
            public void onClick(DialogInterface dialog, int which) {
                dialog.dismiss();
                activity.finish();
            }
        });

        builder.show();
    }

    private boolean downloadFile(String fileName) {
        final String downUrl = "http://nnsuite.mooo.com/warehouse/nnmodels/";

        HttpURLConnection connection = null;
        InputStream input = null;
        OutputStream output = null;
        File downloadFile = null;
        boolean failed = false;

        try {
            String fileUrl = downUrl + fileName;
            URL url = new URL(fileUrl);

            Log.i(TAG, "Start downloading file " + fileName);

            connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(3000);
            connection.setReadTimeout(3000);

            if (connection.getResponseCode() != HttpURLConnection.HTTP_OK) {
                failed = true;
                return false;
            }

            input = new BufferedInputStream(connection.getInputStream(), 8192);

            downloadFile = new File(downloadPath, fileName);
            if (downloadFile.exists()) {
                Log.d(TAG, "File exists, delete this file " + fileName);
                downloadFile.delete();
                downloadFile = new File(downloadPath, fileName);
            }

            output = new FileOutputStream(downloadFile);

            byte[] data = new byte[1024];
            int len;

            while ((len = input.read(data)) > 0) {
                if (isCancelled()) {
                    Log.i(TAG, "Download canceled " + fileName);
                    failed = true;
                    return false;
                }

                output.write(data, 0, len);
            }
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, "Failed to download, " + e.getMessage());
            failed = true;
            return false;
        } finally {
            try {
                if (failed) {
                    Log.i(TAG, "Failed to download file " + fileName);
                }

                if (input != null) {
                    input.close();
                }

                if (output != null) {
                    output.flush();
                    output.close();
                }

                if (downloadFile != null) {
                    if (failed) {
                        downloadFile.delete();
                    } else {
                        Intent intent = new Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE);
                        intent.setData(Uri.fromFile(downloadFile));
                        activity.sendBroadcast(intent);
                    }
                }

                if (connection != null) {
                    connection.disconnect();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }

        return !failed;
    }
}
