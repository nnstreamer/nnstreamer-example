package org.nnsuite.nnstreamer.sample;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.TextView;

import org.nnsuite.nnstreamer.NNStreamer;
import org.nnsuite.nnstreamer.Pipeline;
import org.nnsuite.nnstreamer.TensorsData;
import org.nnsuite.nnstreamer.TensorsInfo;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;


public class MXNetActivity extends Activity {
    private static final String TAG = "NNStreamer-JAVA-Inception-Sample";
    private static final int PERMISSION_REQUEST_CODE = 3;
    private static final String[] requiredPermissions = new String[] {
            Manifest.permission.CAMERA,
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE
    };
    private boolean initialized = false;

    private SurfaceView mCameraView;
    private SurfaceView mResultView;
    private TextView mTextView;
    private Camera mCamera;

    private Pipeline currentPipe;
    private File mxnetModel;
    private File labelFile;
    private SurfaceHolder.Callback prevCameraCallback;
    private SurfaceHolder.Callback prevScaledCallback;
    String path;

    private int updateFrame = 0;
    private int updateLimit = 60;

    private ArrayList<String> textLabels = new ArrayList<>();

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

        setContentView(R.layout.activity_mxnet);
        mCameraView = findViewById(R.id.cameraView);
        mResultView = findViewById(R.id.resultView);
        mTextView = findViewById(R.id.textView);

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

    public static boolean isValidBuffer(ByteBuffer buffer, int expected) {
        if (buffer != null && buffer.isDirect() && buffer.order() == ByteOrder.nativeOrder()) {
            int capacity = buffer.capacity();

            return (capacity == expected);
        }

        return false;
    }

    /**
     * Prepare camera preview.
     */
    private void preparePreview() {
        copyFilesToExternalDir();
        path = getExternalFilesDir(null).getAbsolutePath();
        mxnetModel = new File(path+"/Inception-BN.json");
        labelFile = new File(path+"/labels.txt");

        currentPipe = new Pipeline(getDesc());
        mCamera = Camera.open();

        resetView();
        resetCamera();

        mCamera.setPreviewCallback(getNewPreviewCallback());

        // Read text labels
        try {
            FileReader fr = new FileReader(labelFile);
            BufferedReader reader = new BufferedReader(fr);
            String name;
            while ((name = reader.readLine()) != null) {
                textLabels.add(name);
            }
        } catch (FileNotFoundException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        }

        currentPipe.registerSinkCallback("sinkx", new Pipeline.NewDataCallback() {
            @Override
            public void onNewDataReceived(TensorsData data) {
                if (data == null || data.getTensorsCount() != 1) {
                    return;
                }

                ByteBuffer buffer = data.getTensorData(0);
                if (isValidBuffer(buffer, 4)) {
                    final int index = buffer.getInt(0);
                    runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            mTextView.setText(textLabels.get(index));
                        }
                    });
                }
            }
        });
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
        mResultView.getHolder().addCallback(prevScaledCallback);
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
                Log.d(TAG, "ResultView surface created");
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                Log.d(TAG, "ResultView surface changed");
                getCurrentPipe().setSurface("sink_result", holder.getSurface());
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.d(TAG, "ResultView surface destroyed");
            }
        };
    }

    private Camera.PreviewCallback getNewPreviewCallback()
    {
        return new Camera.PreviewCallback() {
            @Override
            public void onPreviewFrame(byte[] data, Camera camera) {
                updateFrame += 1;
                if(updateFrame == 1)
                {
                    TensorsInfo info = new TensorsInfo();
                    info.addTensorInfo(NNStreamer.TensorType.UINT8, new int[]{3,640,480,1});
                    TensorsData input = info.allocate();
                    ByteBuffer buffer = input.getTensorData(0);
                    buffer.put(Arrays.copyOf(data, data.length));
                    input.setTensorData(0, buffer);
                    getCurrentPipe().inputData("srcx", input);

                    getCurrentPipe().stop();
                }
                if(updateFrame > updateLimit)
                {
                    currentPipe.start();
                    updateFrame = 0;
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
        String desc = "appsrc name=srcx  ! " +
                "video/x-raw,format=NV21,width=640,height=480,framerate=(fraction)0/1 ! tee name=t_raw " +
                "t_raw. ! queue ! videoflip method=clockwise ! glimagesink name=sink_result " +
                "t_raw. ! queue ! videoconvert ! video/x-raw,format=RGB,width=640,height=480,framerate=(fraction)0/1 ! videoscale ! video/x-raw,format=RGB,width=224,height=224 ! tensor_converter ! " +
                "tensor_transform mode=arithmetic option=typecast:float32,per-channel:true@0,add:-123.68@0,add:-116.779@1,add:-103.939@2 ! " +
                "tensor_transform mode=dimchg option=0:2 ! " +
                "tensor_filter framework=mxnet model=" + mxnetModel.getAbsolutePath() + " input=1:3:224:224 inputtype=float32 inputname=data output=1 outputtype=float32 outputname=argmax_channel custom=input_rank=4,enable_tensorrt=false latency=1 ! " +
                "tensor_transform mode=arithmetic option=typecast:int32,add:0 acceleration=false !" +
                "tensor_sink name=sinkx";

        return desc;
    }
}
