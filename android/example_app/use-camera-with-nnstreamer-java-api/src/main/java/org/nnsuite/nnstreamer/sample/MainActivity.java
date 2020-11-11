package org.nnsuite.nnstreamer.sample;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.ImageFormat;
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

import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;

/**
 * Sample code to run the application with nnstreamer-api.
 * Before building this sample, copy nnstreamer-api library file (nnstreamer-YYYY-MM-DD.aar) into 'libs' directory.
 */
public class MainActivity extends Activity {
    private static final String TAG = "NNStreamer-JAVA-CAMERA-Sample";
    private static final int PERMISSION_REQUEST_CODE = 3;
    private static final String[] requiredPermissions = new String[] {
            Manifest.permission.CAMERA,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE
    };
    private boolean initialized = false;

    private ImageView mScaledView;
    private ImageView mFlippedView;
    private SurfaceView mCameraView;
    private Camera mCamera;

    private Pipeline pipe;
    private TensorsInfo info;

    private void initCamera() {
        mCamera = Camera.open();

        Camera.Parameters parameters = mCamera.getParameters();
        List<String> focusModes = parameters.getSupportedFocusModes();
        if (focusModes.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)){
            Log.d(TAG, "Set FOCUS_MODE_CONTINUOUS_PICTURE");
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
        }

        parameters.setPreviewSize(640, 480);
        parameters.setPreviewFormat(ImageFormat.NV21);
        parameters.setPreviewFrameRate(30);

        mCamera.setParameters(parameters);
        mCamera.setDisplayOrientation(90);
        mCamera.setPreviewCallback(new Camera.PreviewCallback() {
            @Override
            public void onPreviewFrame(byte[] data, Camera camera) {
                TensorsData input = info.allocate();
                ByteBuffer buffer = input.getTensorData(0);
                buffer.put(Arrays.copyOf(data, data.length));
                input.setTensorData(0, buffer);
                pipe.inputData("srcx", input);
            }
        });
    }

    private void initView() {
        mCameraView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.d(TAG, "Preview surface created");
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                Log.d(TAG, "Preview surface changed");

                try {
                    mCamera.stopPreview();

                    mCamera.setPreviewDisplay(holder);
                    mCamera.startPreview();
                    Log.d(TAG, "Preview resumed.");
                } catch (Exception e) {
                    Log.e(TAG, "Error on starting camera preview: " + e.getMessage());
                    e.printStackTrace();
                }
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.d(TAG, "Preview surface destroyed");

                if (mCamera != null) {
                    mCamera.stopPreview();
                }
            }
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initNNStreamer();

        setContentView(R.layout.activity_main);

        mCameraView = findViewById(R.id.cameraView);
        mScaledView = findViewById(R.id.scaled_result);
        mFlippedView = findViewById(R.id.flipped_result);

        /* check permissions */
        for (String permission : requiredPermissions) {
            if (!checkPermission(permission)) {
                ActivityCompat.requestPermissions(this,
                        requiredPermissions, PERMISSION_REQUEST_CODE);
                return;
            }
        }

        preparePreview();
    }

    @Override
    protected void onResume() {
        super.onResume();

        if (pipe != null) {
            pipe.start();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();

        if (pipe != null) {
            pipe.stop();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (pipe != null) {
            pipe.close();
        }

        if (mCamera != null) {
            mCamera.stopPreview();
            mCamera.setPreviewCallback(null);
            mCamera.release();
            mCamera = null;
        }
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
     * Prepare camera preview.
     */
    private void preparePreview() {
        final String desc = "appsrc name=srcx ! " +
                "video/x-raw,format=NV21,width=640,height=480,framerate=(fraction)30/1 ! " +
                "videoflip method=clockwise ! tee name=t " +
                "t. ! queue ! videoconvert ! videoscale ! video/x-raw,width=320,height=240 ! " +
                    "tensor_converter ! tensor_sink name=sinkscaled " +
                "t. ! queue ! videoflip method=clockwise ! videoconvert ! " +
                    "tensor_converter ! tensor_sink name=sinkflipped";

        pipe = new Pipeline(desc);

        /* The preview has format of (NV21 640*480) */
        info = new TensorsInfo();
        info.addTensorInfo(NNStreamer.TensorType.UINT8, new int[]{(int) (1.5 * 640 * 480), 1, 1, 1});

        /* register sink callback for scale */
        pipe.registerSinkCallback("sinkscaled", new Pipeline.NewDataCallback() {
            @Override
            public void onNewDataReceived(TensorsData data) {
                try {
                    Bitmap bitmap = Bitmap.createBitmap(320, 240, Bitmap.Config.ARGB_8888);
                    bitmap.copyPixelsFromBuffer(data.getTensorData(0));

                    mScaledView.setImageBitmap(bitmap);
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
                    Bitmap bitmap = Bitmap.createBitmap(640, 480, Bitmap.Config.ARGB_8888);
                    bitmap.copyPixelsFromBuffer(data.getTensorData(0));

                    mFlippedView.setImageBitmap(bitmap);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });

        initCamera();
        initView();
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
