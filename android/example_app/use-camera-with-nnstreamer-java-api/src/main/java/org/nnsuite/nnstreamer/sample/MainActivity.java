package org.nnsuite.nnstreamer.sample;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.hardware.Camera;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.ImageView;

import org.nnsuite.nnstreamer.NNStreamer;
import org.nnsuite.nnstreamer.Pipeline;
import org.nnsuite.nnstreamer.TensorsData;
import org.nnsuite.nnstreamer.TensorsInfo;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;

/**
 * Sample code to run the application with nnstreamer-api.
 * Before building this sample, copy nnstreamer-api library file (nnstreamer-YYYY-MM-DD.aar) into 'libs' directory.
 */
public class MainActivity extends Activity implements SurfaceHolder.Callback, Camera.PreviewCallback {
    private static final String TAG = "NNStreamer-JAVA-CAMERA-Sample";
    private static final int PERMISSION_REQUEST_CODE = 3;
    private static final String[] requiredPermissions = new String[] {
            Manifest.permission.CAMERA,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE
    };
    private boolean initialized = false;

    private ImageView mScaledView;
    private Bitmap scaledBitmap;

    private ImageView mFlippedView;
    private Bitmap flippedBitmap;

    private SurfaceView mCameraView;
    private SurfaceHolder mCameraHolder;
    private Camera mCamera;

    private String desc = "appsrc name=srcx ! " +
        "video/x-raw,format=NV21,width=640,height=480,framerate=(fraction)30/1 ! videoflip method=clockwise ! " +
        "tee name=t " +
            "t. ! queue ! videoconvert ! videoscale ! video/x-raw,width=640,height=480 ! " +
                "tensor_converter ! tensor_sink name=sinkscaled " +
            "t. ! queue ! videoflip method=clockwise ! videoconvert ! videoscale ! video/x-raw,width=640,height=480 ! " +
                "tensor_converter ! tensor_sink name=sinkflipped ";

    private Pipeline pipe;
    private TensorsInfo info;

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        TensorsData input = info.allocate();
        ByteBuffer buffer = input.getTensorData(0);
        buffer.put(Arrays.copyOf(data, data.length));
        input.setTensorData(0, buffer);
        pipe.inputData("srcx", input);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Camera.Parameters parameters = mCamera.getParameters();
        List<String> focusModes = parameters.getSupportedFocusModes();
        if (focusModes.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)){
            Log.d(TAG, "Set FOCUS_MODE_CONTINUOUS_PICTURE");
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
        }

        parameters.setPreviewSize(640, 480);
        parameters.setPreviewFormat(17); // NV21. NV21 or YV12 is supported
        parameters.setPreviewFrameRate(30);
        mCamera.setParameters(parameters);

        try {
            if (mCamera == null) {
                mCamera.setPreviewDisplay(holder);
                mCamera.startPreview();
            }
        } catch (IOException e) {

        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (mCameraHolder.getSurface() == null) {
            Log.d(TAG, "Preview surface does not exist");
            return;
        }

        try {
            mCamera.stopPreview();
            Log.d(TAG, "Preview stopped.");
        } catch (Exception e) {
            Log.d(TAG, "Error on stopping camera preview: " + e.getMessage());
        }

        try {
            mCamera.setPreviewDisplay(mCameraHolder);
            mCamera.setPreviewCallback(this);
            mCamera.startPreview();
            Log.d(TAG, "Preview resumed.");
        } catch (Exception e) {
            Log.d(TAG, "Error on starting camera preview: " + e.getMessage());
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (mCamera != null) {
            mCamera.stopPreview();
            mCamera.release();
            mCamera = null;
        }
    }

    private void initCamera() {
        mCamera = Camera.open();
        mCamera.setDisplayOrientation(90);
        mCameraHolder = mCameraView.getHolder();
        mCameraHolder.addCallback(this);
        mCameraHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        mCameraView = (SurfaceView) findViewById(R.id.cameraView);
        mScaledView = (ImageView) findViewById(R.id.scaled_result);
        mFlippedView = (ImageView) findViewById(R.id.flipped_result);

        /* check permissions */
        for (String permission : requiredPermissions) {
            if (!checkPermission(permission)) {
                ActivityCompat.requestPermissions(this,
                        requiredPermissions, PERMISSION_REQUEST_CODE);
                return;
            }
        }
        initCamera();
        initNNStreamer();
        pipe = new Pipeline(desc);
        info = new TensorsInfo();
        /* The preview has format of (NV21 640*480) */
        info.addTensorInfo(NNStreamer.TensorType.UINT8, new int[]{(int) (1.5 * 640 * 480), 1, 1, 1});

        /* register sink callback for scale */
        pipe.registerSinkCallback("sinkscaled", new Pipeline.NewDataCallback() {
            @Override
            public void onNewDataReceived(TensorsData data) {
                try {
                    scaledBitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
                    scaledBitmap.copyPixelsFromBuffer(data.getTensorData(0));
                    tryDrawingScaled(scaledBitmap);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });

        /* register sink callback for flip */
        pipe.registerSinkCallback("sinkflipped", new Pipeline.NewDataCallback() {
            @Override
            public void onNewDataReceived(TensorsData data) {
                try {
                    flippedBitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
                    flippedBitmap.copyPixelsFromBuffer(data.getTensorData(0));
                    tryDrawingFlipped(flippedBitmap);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
    }

    private void tryDrawingScaled(Bitmap bitmap) {
        mScaledView.setImageBitmap(bitmap);
    }


    private void tryDrawingFlipped(Bitmap bitmap) {
        mFlippedView.setImageBitmap(bitmap);
    }

    @Override
    protected void onResume() {
        super.onResume();

        if (initialized) {

            pipe.start();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        pipe.stop();
    }

    /**
     * Check the permission is granted.
     */
    private boolean checkPermission(final String permission) {
        return (ContextCompat.checkSelfPermission(this, permission)
                == PackageManager.PERMISSION_GRANTED);
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
            return;
        }
        finish();
    }

    /**
     * Initialize NNStreamer.
     */
    private void initNNStreamer() {
        if (initialized) {
            return;
        }
        try {
            initialized = NNStreamer.initialize(this);
        } catch(Exception e) {
            e.printStackTrace();
            Log.e(TAG, e.getMessage());
        } finally {
            if (initialized) {
                Log.i(TAG, "Version: " + NNStreamer.getVersion());
            } else {
                Log.e(TAG, "Failed to initialize NNStreamer");
                finish();
            }
        }
    }
}
