package org.nnsuite.nnstreamer.sample;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.nnsuite.nnstreamer.NNStreamer;
import org.nnsuite.nnstreamer.Pipeline;
import org.nnsuite.nnstreamer.TensorsData;
import org.nnsuite.nnstreamer.TensorsInfo;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.List;
import java.io.File;

import java.nio.file.Files;

/**
 * Sample code to run the application with nnstreamer-api.
 * Before building this sample, copy nnstreamer-api library file (nnstreamer-YYYY-MM-DD.aar) into 'libs' directory.
 */
public class MainActivity extends Activity {
    private static final String TAG = "NNStreamer-JAVA-StyleTransfer-Sample";
    private static final int PERMISSION_REQUEST_CODE = 3;
    private static final String[] requiredPermissions = new String[] {
            Manifest.permission.CAMERA,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE
    };
    private boolean initialized = false;

    private SurfaceView mCameraView;
    private SurfaceView mStyledView;
    private Camera mCamera;

    private Pipeline currentPipe;
    private File predictModel;
    private File transferModel;
    private SurfaceHolder.Callback prevCameraCallback;
    private SurfaceHolder.Callback prevScaledCallback;
    String path;

    private int updateFrame = 0;
    private int styleFrame = 0;
    private int updateLimit = 25;
    private int styleNumber = 0;

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

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        initNNStreamer();

        setContentView(R.layout.activity_main);
        mCameraView = findViewById(R.id.cameraView);
        mStyledView = findViewById(R.id.styled_result);

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

        if (getCurrentPipe() != null) {
            getCurrentPipe().start();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();

        if (getCurrentPipe() != null) {
            getCurrentPipe().stop();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        if (getCurrentPipe() != null) {
            getCurrentPipe().close();
            currentPipe = null;
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
        copyFilesToExternalDir();
        path = getExternalFilesDir(null).getAbsolutePath();
        predictModel = new File(path+"/style_predict_quantized_256.tflite");
        transferModel = new File(path+"/style_transfer_quantized_384.tflite");

        currentPipe = new Pipeline(getDesc());
        mCamera = Camera.open();

        resetView();
        resetCamera();
    }

    private void copyFilesToExternalDir()
    {
        AssetManager am = getResources().getAssets();
        String[] files = null;

        try {
            files = am.list("models/");
        } catch (Exception e) {
            Log.e("TAG", "#### Failed to get asset file list");
            e.printStackTrace();
            return;
        }

        // Copy files into app-specific directory.
        for (String filename : files) {
            try {
                InputStream in = am.open("models/" + filename);
                String outDir = getExternalFilesDir(null).getAbsolutePath();
                File outFile = new File(outDir, filename);
                OutputStream out = new FileOutputStream(outFile);

                byte[] buffer = new byte[1024];
                int read;
                while ((read = in.read(buffer)) != -1) {
                    out.write(buffer, 0, read);
                }

                in.close();
                out.flush();
                out.close();
            } catch (IOException e) {
                Log.e("TAG", "Failed to copy file: " + filename);
                e.printStackTrace();
                return;
            }
        }
    }

    private void resetView() {
        prevCameraCallback = getNewCameraCallback();
        mCameraView.getHolder().addCallback(prevCameraCallback);

        prevScaledCallback = getNewScaledCallback();
        mStyledView.getHolder().addCallback(prevScaledCallback);
    }

    private void resetCamera()
    {
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
    }

    private SurfaceHolder.Callback getNewCameraCallback()
    {
        return new SurfaceHolder.Callback() {
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
                    mCamera.setPreviewCallback(getNewPreviewCallback());
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
        };
    }

    private SurfaceHolder.Callback getNewScaledCallback()
    {
        return new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.d(TAG, "Styledview surface created");
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                Log.d(TAG, "Styledview surface changed");
                getCurrentPipe().setSurface("sinkstyled", holder.getSurface());
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.d(TAG, "Styledview surface destroyed");
            }
        };
    }

    private Camera.PreviewCallback getNewPreviewCallback()
    {
        return new Camera.PreviewCallback() {
            @Override
            public void onPreviewFrame(byte[] data, Camera camera) {
                updateFrame += 1;
                styleFrame += 1;
                if(updateFrame == 1)
                {
                    TensorsInfo info = new TensorsInfo();
                    info.addTensorInfo(NNStreamer.TensorType.UINT8, new int[]{3,640,480,1});
                    TensorsData input = info.allocate();
                    ByteBuffer buffer = input.getTensorData(0);
                    buffer.put(Arrays.copyOf(data, data.length));
                    input.setTensorData(0, buffer);
                    getCurrentPipe().inputData("srcx", input);

                    TensorsInfo info1 = new TensorsInfo();
                    info1.addTensorInfo(NNStreamer.TensorType.UINT8, new int[]{3,256,256,1});
                    TensorsData input1 = info1.allocate();
                    ByteBuffer buffer1 = input1.getTensorData(0);
                    byte[] data1 = null;
                    try {
                        data1 = Files.readAllBytes(getCurrentStyleRawImage().toPath());
                    }
                    catch (Exception e)
                    {
                        e.printStackTrace();
                    }
                    buffer1.put(Arrays.copyOf(data1, data1.length));
                    input1.setTensorData(0, buffer1);
                    getCurrentPipe().inputData("src1", input1);

                    getCurrentPipe().stop();
                }
                if(updateFrame > updateLimit)
                {
                    currentPipe.start();
                    updateFrame = 0;
                }
                if(styleFrame > 300)
                {
                    styleNumber++;
                    Log.d(TAG, "#### update style to style "+styleNumber+".");
                    styleFrame = 0;
                    updateFrame -= 10;
                }
            }
        };
    }

    private Pipeline getCurrentPipe()
    {
        return currentPipe;
    }

    private String getDesc()
    {
        String desc = "appsrc name=srcx ! " +
                "video/x-raw,format=NV21,width=640,height=480,framerate=(fraction)0/1 ! " +
                "videoconvert ! " +
                "video/x-raw,format=RGB,width=640,height=480,framerate=(fraction)0/1 ! " +
                "videoflip method=clockwise ! videocrop left=0 right=0 top=80 bottom=80 ! " +
                "videoconvert ! videoscale ! video/x-raw,width=384,height=384 ! tensor_converter ! " +
                "other/tensor,dimension=(string)3:384:384:1,type=(string)uint8,framerate=(fraction)0/1 ! " +
                "tensor_transform mode=arithmetic option=typecast:float32,add:0.0,div:255.0 ! mux.sink_0 "+
                "appsrc name=src1 ! " +
                "other/tensor,dimension=(string)3:256:256:1,type=(string)uint8,framerate=(fraction)0/1 ! " +
                "tensor_transform mode=arithmetic option=typecast:float32,add:0.0,div:255.0 ! " +
                "tensor_filter framework=tensorflow-lite model=" + predictModel.getAbsolutePath() + " ! " +
                "mux.sink_1 " +
                "tensor_mux name=mux sync_mode=nosync ! tensor_filter framework=tensorflow-lite model=" + transferModel.getAbsolutePath() +" ! " +
                "tensor_transform mode=arithmetic option=mul:255.0,add:0.0 ! " +
                "tensor_transform mode=typecast option=uint8 ! " +
                "tensor_decoder mode=direct_video ! " +
                "videoconvert ! " +
                "video/x-raw,format=RGB,width=384,height=384,framerate=(fraction)0/1 ! " +
                "glimagesink name=sinkstyled";

        return desc;
    }

    private File getCurrentStyleRawImage()
    {
        String path = getExternalFilesDir(null).getAbsolutePath();
        return new File(path+"/style"+styleNumber+".raw");
    }
}
