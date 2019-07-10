package org.freedesktop.nnstreamer.nnstreamermultidevice;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.os.Environment;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.text.Html;
import android.text.InputType;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.LinearLayout;
import android.widget.ToggleButton;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;
import java.util.ArrayList;
import java.io.File;
import java.util.Locale;

import org.freedesktop.gstreamer.GStreamer;
import org.freedesktop.gstreamer.GStreamerSurfaceView;

public class NNStreamerMultiDevice extends Activity implements SurfaceHolder.Callback {
    private static final int PERMISSION_REQUEST_ALL = 3;
    private static final String TAG = "NNStreamer";
    private static final String downloadPath = Environment.getExternalStorageDirectory().getPath()
            + "/nnstreamer/tflite_model";

    private DownloadModel downloadTask = null;
    private ArrayList<String> downloadList = new ArrayList<>();

    final Context context = this;
    private TextView viewDesc;

    private native void nativeInit(Context context, int w, int h);     // Initialize native code, build pipeline, etc

    private native void nativeFinalize(); // Destroy pipeline and shutdown native code
    private native void nativeStart(String ipadd, String ipadd_sub, int id, int port, int port_sub ); /* Start pipeline with id */
    private native void nativeStop();     /* Stop the pipeline */
    private native void nativePlay();     // Set pipeline to PLAYING
    private native void nativePause();    // Set pipeline to PAUSED
    private static native boolean nativeClassInit(); // Initialize native class: cache Method IDs for callbacks

    private native void nativeSurfaceInit(Object surface);

    private native void nativeSurfaceFinalize();

    private long native_custom_data;      // Native code will use this to keep private data

    private boolean is_playing_desired;   // Whether the user asked to go to PLAYING

    private ToggleButton button_s, button_obj, button_pose;
    private EditText res_ip, res_port, res_sub_ip, res_sub_port;

    private boolean initialized = false;
    // Called when the activity is first created.
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);



        if (!checkPermission(Manifest.permission.CAMERA) ||
	    !checkPermission(Manifest.permission.READ_EXTERNAL_STORAGE) ||
	    !checkPermission(Manifest.permission.WRITE_EXTERNAL_STORAGE)) {
            ActivityCompat.requestPermissions(this,
					      new String[]{
						  Manifest.permission.CAMERA,
						  Manifest.permission.READ_EXTERNAL_STORAGE,
						  Manifest.permission.WRITE_EXTERNAL_STORAGE
					      }, PERMISSION_REQUEST_ALL);
            return;
        }
        initActivity();
    }

    @Override
    public void onPause(){
        super.onPause();
        nativePause();
    }

    @Override
    public void onResume(){
        super.onResume();
        if(initialized){
            if (downloadTask != null && downloadTask.isProgress()) {
                Log.d(TAG, "Now downloading model files");
            } else {
		nativePlay();
            }

        }

    }


    private String getLocalIpAddress(){
	String ipAddress = null;
	WifiManager wm = (WifiManager) getSystemService(WIFI_SERVICE);
	WifiInfo myWifiInfo = wm.getConnectionInfo();
	if(myWifiInfo != null) {
		int addr = myWifiInfo.getIpAddress();
		ipAddress = String.format(Locale.US,
				"%d.%d.%d.%d",
				(addr & 0xff),
				(addr >> 8 & 0xff),
				(addr >> 16 & 0xff),
				(addr >> 24 & 0xff));
	}
	return ipAddress;
    }

    public void enableButton(boolean enabled, int id ) {
	int i;
	button_s.setEnabled(enabled);
	button_obj.setEnabled(enabled);
	button_pose.setEnabled(enabled);

	if(!enabled){
	    switch(id){
	    case 0:
		button_s.setBackgroundColor(Color.parseColor("#e1f5fe"));
		button_s.setTextColor(Color.GRAY);
		button_s.setText("Camera Input Generation");
		break;
	    case 1:
		button_obj.setBackgroundColor(Color.parseColor("#d5f5e2"));
		button_obj.setTextColor(Color.GRAY);
		button_obj.setText("Object Detection");
		break;
	    case 2:
		button_pose.setBackgroundColor(Color.parseColor("#fadbd8"));
		button_pose.setTextColor(Color.GRAY);
		button_pose.setText("Pose Estimation");
		break;
	    }

	    if(id != 0) button_s.setVisibility(View.GONE);
	    if(id != 1) button_obj.setVisibility(View.GONE);
	    if(id != 2) button_pose.setVisibility(View.GONE);
	}
    }

    private boolean checkModelFile(String fileName) {
        File modelFile;

        modelFile = new File(downloadPath, fileName);
        if (!modelFile.exists()) {
            Log.d(TAG, "Cannot find model file " + fileName);
            downloadList.add(fileName);
            return false;
        }

        return true;
    }


    /**
     * Start to download model files.
     */
    private void downloadModels() {
        downloadTask = new DownloadModel(this, downloadPath);
        downloadTask.execute(downloadList);
    }

    private boolean checkModels() {
        downloadList.clear();

        checkModelFile("box_priors.txt");
        checkModelFile("ssd_mobilenet_v2_coco.tflite");
        checkModelFile("coco_labels_list.txt");
        checkModelFile("detect_face.tflite");
        checkModelFile("labels_face.txt");
        checkModelFile("detect_hand.tflite");
        checkModelFile("labels_hand.txt");
        checkModelFile("detect_pose.tflite");

        return !(downloadList.size() > 0);
    }

    /**
     * Show dialog to download model files.
     */
    private void showDownloadDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);

        builder.setCancelable(false);
        builder.setMessage(R.string.download_model_file);

        builder.setNegativeButton(R.string.cancel, new DialogInterface.OnClickListener() {
		@Override
		public void onClick(DialogInterface dialog, int which) {
		    dialog.dismiss();
		    finish();
		}
	    });

        builder.setPositiveButton(R.string.download, new DialogInterface.OnClickListener() {
		@Override
		public void onClick(DialogInterface dialog, int which) {
		    dialog.dismiss();
		    downloadModels();
		}
	    });

        builder.show();
    }

    private void initActivity() {
        // Initialize GStreamer and warn if it fails
        if(initialized){
            return;
        }
        try {
            GStreamer.init(this);
        } catch (Exception e) {
            Toast.makeText(this, e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        setContentView(R.layout.main);


        res_ip = (EditText) findViewById(R.id.editTextIP);
        res_port = (EditText) findViewById(R.id.editTextPort);
        res_sub_ip = (EditText) findViewById(R.id.editTextSubIP);
        res_sub_port = (EditText) findViewById(R.id.editTextSubPort);
	res_ip.setInputType(InputType.TYPE_NULL);
	res_port.setInputType(InputType.TYPE_NULL);
	res_sub_ip.setInputType(InputType.TYPE_NULL);
	res_sub_port.setInputType(InputType.TYPE_NULL);
	LinearLayout layer = (LinearLayout) findViewById(R.id.subLayout);
	layer.setVisibility(View.GONE);

	viewDesc = (TextView) findViewById(R.id.main_text_desc);

        button_s = (ToggleButton) findViewById(R.id.button_pre);
	button_s.setText("preproc.");
	button_s.setTextOn("preproc.");
	button_s.setTextOff("preproc.");
        button_s.setOnClickListener(new OnClickListener() {
		@Override
		public void onClick(View arg0) {
		    LayoutInflater li= LayoutInflater.from(context);
		    View promptsView = li.inflate(R.layout.prompts,null);
		    AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(context);

		    alertDialogBuilder.setView(promptsView);
		    LinearLayout sublayer = (LinearLayout) promptsView.findViewById(R.id.subLayout);
		    sublayer.setVisibility(View.GONE);
		    sublayer = (LinearLayout) promptsView.findViewById(R.id.subtextLayout);
		    sublayer.setVisibility(View.GONE);

		    final EditText ipaddr = (EditText) promptsView.findViewById(R.id.editTextIP);
		    final EditText portnum = (EditText) promptsView.findViewById(R.id.editTextPort);
		    ipaddr.setText(getLocalIpAddress());
		    ipaddr.setEnabled(false);
		    portnum.setText("4000");

		    alertDialogBuilder
			.setTitle("Preprocessing Image")
			.setMessage("Please put port number for this device")
			.setCancelable(false)
			.setPositiveButton("OK",
					   new DialogInterface.OnClickListener() {
					       @Override
					       public void onClick(DialogInterface dialog, int id) {
						   res_ip.setText(ipaddr.getText());
						   res_port.setText(portnum.getText());
						   res_ip.setEnabled(false);
						   res_port.setEnabled(false);
						   enableButton(false,0);
						   if(!checkModels()){
						       showDownloadDialog();
						   }
						   nativeStart(res_ip.getText().toString(), null, 0,
                                   Integer.parseInt(res_port.getText().toString()), 0);
					       }
					   })
                        .setNegativeButton("Cancel",
					   new DialogInterface.OnClickListener() {
					       @Override
					       public void onClick(DialogInterface dialog, int id) {
						   dialog.cancel();
					       }
					   });
		    AlertDialog alertDialog = alertDialogBuilder.create();
		    alertDialog.show();
		}
	    });

        button_obj = (ToggleButton) findViewById(R.id.button_obj);
	button_obj.setText("obj detec");
	button_obj.setTextOn("obj detec");
	button_obj.setTextOff("obj detec.");

        button_obj.setOnClickListener(new OnClickListener() {
		@Override
		public void onClick(View arg0) {
		    LayoutInflater li= LayoutInflater.from(context);
		    View promptsView = li.inflate(R.layout.prompts,null);
		    AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(context);
		    LinearLayout layer = (LinearLayout) findViewById(R.id.subLayout);
		    layer.setVisibility(View.VISIBLE);

		    alertDialogBuilder.setView(promptsView);

		    final EditText ipaddr = (EditText) promptsView.findViewById(R.id.editTextIP);
		    final EditText portnum = (EditText) promptsView.findViewById(R.id.editTextPort);
		    ipaddr.setText("192.168.0.6");
		    portnum.setText("4000");


		    final EditText ipaddr_sub = (EditText) promptsView.findViewById(R.id.editTextIP_sub);
		    final EditText portnum_sub = (EditText) promptsView.findViewById(R.id.editTextPort_sub);
		    ipaddr_sub.setText(getLocalIpAddress());
		    ipaddr_sub.setEnabled(false);
		    portnum_sub.setText("5000");

		    alertDialogBuilder
			.setTitle("Object Detection")
			.setMessage("Please put preprocessing device ip addr & port number to get the" +
                    " result of preporcessing")
			.setCancelable(false)
			.setPositiveButton("OK",
					   new DialogInterface.OnClickListener() {
					       @Override
					       public void onClick(DialogInterface dialog, int id) {
						   res_ip.setText(ipaddr.getText());
						   res_port.setText(portnum.getText());
						   res_sub_ip.setText(ipaddr_sub.getText());
						   res_sub_port.setText(portnum_sub.getText());
						   res_ip.setEnabled(false);
						   res_port.setEnabled(false);
						   res_sub_ip.setEnabled(false);
						   res_sub_port.setEnabled(false);
						   enableButton(false, 1);
						   if(!checkModels()){
						       showDownloadDialog();
						   }

						   nativeStart(res_ip.getText().toString(), res_sub_ip.getText().toString(),
                                   1, Integer.parseInt(res_port.getText().toString()),
                                   Integer.parseInt(res_sub_port.getText().toString()));
					       }
					   })
                        .setNegativeButton("Cancel",
					   new DialogInterface.OnClickListener() {
					       @Override
					       public void onClick(DialogInterface dialog, int id) {
						   LinearLayout layer = (LinearLayout) findViewById(R.id.subLayout);
						   layer.setVisibility(View.GONE);

						   dialog.cancel();
					       }
					   });
		    AlertDialog alertDialog = alertDialogBuilder.create();
		    alertDialog.show();
		}
	    });

        button_pose = (ToggleButton) findViewById(R.id.button_pose);
	button_pose.setText("pose est.");
	button_pose.setTextOn("pose est.");
	button_pose.setTextOff("pose est..");
        button_pose.setOnClickListener(new OnClickListener() {
		@Override
		public void onClick(View arg0) {
		    LayoutInflater li= LayoutInflater.from(context);
		    View promptsView = li.inflate(R.layout.prompts,null);
		    AlertDialog.Builder alertDialogBuilder = new AlertDialog.Builder(context);

		    alertDialogBuilder.setView(promptsView);
		    LinearLayout sublayer = (LinearLayout) promptsView.findViewById(R.id.subLayout);
		    sublayer.setVisibility(View.GONE);
		    sublayer = (LinearLayout) promptsView.findViewById(R.id.subtextLayout);
		    sublayer.setVisibility(View.GONE);

		    final EditText ipaddr = (EditText) promptsView.findViewById(R.id.editTextIP);
		    final EditText portnum = (EditText) promptsView.findViewById(R.id.editTextPort);
		    ipaddr.setText("192.168.0.5");
		    portnum.setText("5000");

		    alertDialogBuilder
			.setTitle("Pose Estimation")
			.setMessage("Please put preprocessing device ip addr & port number to get the" +
                    " result of preporcessing")
			.setCancelable(false)
			.setPositiveButton("OK",
					   new DialogInterface.OnClickListener() {
					       @Override
					       public void onClick(DialogInterface dialog, int id) {
						   res_ip.setText(ipaddr.getText());
						   res_port.setText(portnum.getText());
						   res_ip.setEnabled(false);
						   res_port.setEnabled(false);
						   enableButton(false, 2);
						   if(!checkModels()){
						       showDownloadDialog();
						   }
						   nativeStart(res_ip.getText().toString(), null, 2,
                                   Integer.parseInt(res_port.getText().toString()), 0);
					       }
					   })
                        .setNegativeButton("Cancel",
					   new DialogInterface.OnClickListener() {
					       @Override
					       public void onClick(DialogInterface dialog, int id) {
						   dialog.cancel();
					       }
					   });
		    AlertDialog alertDialog = alertDialogBuilder.create();
		    alertDialog.show();
		}
	    });

        SurfaceView sv = (SurfaceView) this.findViewById(R.id.surface_video);
        SurfaceHolder sh = sv.getHolder();
        sh.addCallback(this);

        nativeInit(this, GStreamerSurfaceView.media_width, GStreamerSurfaceView.media_height);
        initialized = true;
    }


    private boolean checkPermission(final String permission) {
        return (ContextCompat.checkSelfPermission(this, permission)
                == PackageManager.PERMISSION_GRANTED);
    }
    protected void onSaveInstanceState (Bundle outState) {
        Log.d ("GStreamer", "Saving state, playing:" + is_playing_desired);
        outState.putBoolean("playing", is_playing_desired);
    }

    protected void onDestroy() {
        nativeFinalize();
        super.onDestroy();
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

            initActivity();
            return;
        }

        finish();
    }


    // Called from native code. This sets the content of the TextView from the UI thread.
    private void setMessage(final String message) {
        final TextView tv = (TextView) this.findViewById(R.id.textview_message);
        runOnUiThread (new Runnable() {
		public void run() {
		    tv.setText(message);
		}
	    });
    }

    // Called from native code. Native code calls this once it has created its pipeline and
    // the main loop is running, so it is ready to accept commands.
    private void onGStreamerInitialized (final String desc) {
	Log.i ("GStreamer", "Gst initialized. Restoring state, playing:" + is_playing_desired);

	// Restore previous playing state
	if (is_playing_desired) {
	    nativePlay();
	} else {
	    nativePause();
	}

	// Re-enable buttons, now that GStreamer is initialized
	final Activity activity = this;
	runOnUiThread(new Runnable() {
		public void run() {
		    Log.d("GStreamer",desc);
		    viewDesc.setText(Html.fromHtml(desc, Html.FROM_HTML_MODE_LEGACY));
		    nativePlay();
		}
	    });
    }

    static {
	System.loadLibrary("gstreamer_android");
	System.loadLibrary("nnstreamer_multidevice");
	nativeClassInit();
    }

    public void surfaceChanged(SurfaceHolder holder, int format, int width,
			       int height) {
        Log.d("GStreamer", "Surface changed to format " + format + " width "
	      + width + " height " + height);
        nativeSurfaceInit (holder.getSurface());
    }

    public void surfaceCreated(SurfaceHolder holder) {
        Log.d("GStreamer", "Surface created: " + holder.getSurface());
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d("GStreamer", "Surface destroyed");
        nativeSurfaceFinalize ();
    }
}
