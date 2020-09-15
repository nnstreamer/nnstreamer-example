package org.nnsuite.nnstreamer.sample;

import android.Manifest;
import android.app.Activity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.CountDownTimer;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v4.content.ContextCompat;
import android.util.Log;

import org.nnsuite.nnstreamer.CustomFilter;
import org.nnsuite.nnstreamer.NNStreamer;
import org.nnsuite.nnstreamer.Pipeline;
import org.nnsuite.nnstreamer.SingleShot;
import org.nnsuite.nnstreamer.TensorsData;
import org.nnsuite.nnstreamer.TensorsInfo;

import java.io.File;
import java.nio.ByteBuffer;
import java.nio.file.Files;

/**
 * Sample code to run the application with nnstreamer-api.
 * Before building this sample, copy nnstreamer-api library file into 'libs' directory.
 */
public class MainActivity extends Activity {
    private static final String TAG = "NNStreamer-Sample";

    private static final int PERMISSION_REQUEST_CODE = 3;
    private static final String[] requiredPermissions = new String[] {
            Manifest.permission.READ_EXTERNAL_STORAGE
    };

    private boolean initialized = false;
    private boolean isFailed = false;
    private CountDownTimer exampleTimer = null;
    private int exampleRun = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        /* check permissions */
        for (String permission : requiredPermissions) {
            if (!checkPermission(permission)) {
                ActivityCompat.requestPermissions(this,
                        requiredPermissions, PERMISSION_REQUEST_CODE);
                return;
            }
        }

        initNNStreamer();
    }

    @Override
    public void onResume() {
        super.onResume();

        if (initialized) {
            /* set timer to run examples */
            exampleRun = 0;
            isFailed = false;
            setExampleTimer(200);
        }
    }

    @Override
    public void onPause() {
        super.onPause();

        stopExampleTimer();
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

            initNNStreamer();
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

    /**
     * Set timer to run the examples.
     */
    private void setExampleTimer(long time) {
        stopExampleTimer();

        exampleTimer = new CountDownTimer(time, time) {
            @Override
            public void onTick(long millisUntilFinished) {
            }

            @Override
            public void onFinish() {
                /* run the examples repeatedly */
                if (exampleRun > 7) {
                    Log.d(TAG, "Stop timer to run example");

                    if (isFailed) {
                        Log.d(TAG, "Error occurs while running the examples");
                    }

                    return;
                }

                int option = (exampleRun % 8);

                if (option == 1) {
                    Log.d(TAG, "==== Run pipeline example with state callback ====");
                    runPipe(true);
                } else if (option == 2) {
                    Log.d(TAG, "==== Run pipeline example ====");
                    runPipe(false);
                } else if (option == 3) {
                    Log.d(TAG, "==== Run pipeline example with valve ====");
                    runPipeValve();
                } else if (option == 4) {
                    Log.d(TAG, "==== Run pipeline example with switch ====");
                    runPipeSwitch();
                } else if (option == 5) {
                    Log.d(TAG, "==== Run pipeline example with custom filter ====");
                    runPipeCustomFilter();
                } else if (option == 6) {
                    Log.d(TAG, "==== Run pipeline example with NNFW model ====");
                    runPipeNNFW();
                } else if (option == 7) {
                    Log.d(TAG, "==== Run single-shot example with NNFW model ====");
                    runSingleNNFW();
                } else {
                    Log.d(TAG, "==== Run single-shot example ====");
                    runSingle();
                }

                exampleRun++;
                setExampleTimer(500);
            }
        };

        exampleTimer.start();
    }

    /**
     * Cancel example timer.
     */
    private void stopExampleTimer() {
        if (exampleTimer != null) {
            exampleTimer.cancel();
            exampleTimer = null;
        }
    }

    /**
     * Get the File object of image classification tf-lite model.
     * Note that, to invoke model in the storage, the permission READ_EXTERNAL_STORAGE is required.
     */
    private File getExampleModelTFLite() {
        String root = Environment.getExternalStorageDirectory().getAbsolutePath();
        File model = new File(root + "/nnstreamer/tflite_model_img/mobilenet_v1_1.0_224_quant.tflite");

        if (!model.exists()) {
            Log.e(TAG, "Failed to get the model (image classification)");
            return null;
        }

        return model;
    }

    /**
     * Get the File object for NNFW example (tf-lite).
     * Note that, to invoke model in the storage, the permission READ_EXTERNAL_STORAGE is required.
     */
    private static File getExampleModelNNFW() {
        String root = Environment.getExternalStorageDirectory().getAbsolutePath();
        File model = new File(root + "/nnstreamer/tflite_model_add/add.tflite");
        File meta = new File(root + "/nnstreamer/tflite_model_add/metadata/MANIFEST");

        if (!model.exists() || !meta.exists()) {
            Log.e(TAG, "Failed to get the model (add)");
            return null;
        }

        return model;
    }

    /**
     * Reads raw image file (orange) and returns TensorsData instance.
     */
    private TensorsData readRawImageData() {
        String root = Environment.getExternalStorageDirectory().getAbsolutePath();
        File raw = new File(root + "/nnstreamer/tflite_model_img/orange.raw");

        if (!raw.exists()) {
            Log.e(TAG, "Failed to get the raw image");
            return null;
        }

        TensorsInfo info = new TensorsInfo();
        info.addTensorInfo(NNStreamer.TensorType.UINT8, new int[]{3,224,224,1});

        int size = info.getTensorSize(0);
        TensorsData data;

        try {
            byte[] content = Files.readAllBytes(raw.toPath());
            ByteBuffer buffer = TensorsData.allocateByteBuffer(size);
            buffer.put(content);

            data = TensorsData.allocate(info);
            data.setTensorData(0, buffer);
        } catch (Exception e) {
            Log.e(TAG, "Failed to read the raw image");
            data = null;
        }

        return data;
    }

    /**
     * Gets the label index with max score, for tensorflow-lite image classification.
     */
    private int getMaxScore(ByteBuffer buffer) {
        int index = -1;
        int maxScore = 0;

        if (buffer.capacity() == 1001) {
            for (int i = 0; i < 1001; i++) {
                /* convert unsigned byte */
                int score = (buffer.get(i) & 0xFF);

                if (score > maxScore) {
                    maxScore = score;
                    index = i;
                }
            }
        }

        return index;
    }

    /**
     * Print tensors info.
     */
    private void printTensorsInfo(TensorsInfo info) {
        int num = info.getTensorsCount();

        Log.d(TAG, "The number of tensors in info: " + num);
        for (int i = 0; i < num; i++) {
            int[] dim = info.getTensorDimension(i);

            Log.d(TAG, "Info index " + i +
                    " name: " + info.getTensorName(0) +
                    " type: " + info.getTensorType(0) +
                    " dim: " + dim[0] + ":" + dim[1] + ":" + dim[2] + ":" + dim[3]);
        }
    }

    /**
     * Print tensors data.
     *
     * The maximum number of tensor instances that tensors may have.
     * {@link NNStreamer#TENSOR_SIZE_LIMIT}
     */
    private void printTensorsData(TensorsData data) {
        int num = data.getTensorsCount();

        Log.d(TAG, "The number of tensors in data: " + num);
        for (int i = 0; i < num; i++) {
            ByteBuffer buffer = data.getTensorData(i);

            Log.d(TAG, "Data index " + i + " received " + buffer.capacity());
        }
    }

    /**
     * Example to run single-shot.
     */
    private void runSingle() {
        /* example with image classification tf-lite model */
        File model = getExampleModelTFLite();

        if (model == null || !model.exists()) {
            Log.w(TAG, "Cannot find the model file");
            return;
        }

        try {
            SingleShot single = new SingleShot(model);

            Log.d(TAG, "Get input tensors info");
            TensorsInfo inInfo = single.getInputInfo();
            printTensorsInfo(inInfo);

            Log.d(TAG, "Get output tensors info");
            TensorsInfo outInfo = single.getOutputInfo();
            printTensorsInfo(outInfo);

            /* set timeout (1 second) */
            single.setTimeout(1000);

            /* single-shot invoke */
            TensorsData in = readRawImageData();

            if (in != null) {
                TensorsData out = single.invoke(in);
                int labelIndex = getMaxScore(out.getTensorData(0));

                /* check label index (orange) */
                Log.d(TAG, "The label index is " + labelIndex);
                if (labelIndex != 951) {
                    Log.w(TAG, "The raw image is not orange!");
                    isFailed = true;
                }
            }

            single.close();
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, e.getMessage());
            isFailed = true;
        }
    }

    /**
     * Example to run pipeline.
     */
    private void runPipe(boolean addStateCb) {
        /* example with image classification tf-lite model */
        File model = getExampleModelTFLite();

        if (model == null || !model.exists()) {
            Log.w(TAG, "Cannot find the model file");
            return;
        }

        try {
            String desc = "appsrc name=srcx ! " +
                    "other/tensor,dimension=(string)3:224:224:1,type=(string)uint8,framerate=(fraction)0/1 ! " +
                    "tensor_filter framework=tensorflow-lite model=" + model.getAbsolutePath() + " ! " +
                    "tensor_sink name=sinkx";

            /* pipeline state callback */
            Pipeline.StateChangeCallback stateCb = null;

            if (addStateCb) {
                stateCb = new Pipeline.StateChangeCallback() {
                    @Override
                    public void onStateChanged(Pipeline.State state) {
                        Log.d(TAG, "The pipeline state changed to " + state);
                    }
                };
            }

            Pipeline pipe = new Pipeline(desc, stateCb);

            /* register sink callback */
            pipe.registerSinkCallback("sinkx", new Pipeline.NewDataCallback() {
                int received = 0;

                @Override
                public void onNewDataReceived(TensorsData data) {
                    Log.d(TAG, "Received new data callback " + (++received));

                    printTensorsInfo(data.getTensorsInfo());
                    printTensorsData(data);

                    int labelIndex = getMaxScore(data.getTensorData(0));

                    /* check label index (orange) */
                    Log.d(TAG, "The label index is " + labelIndex);
                    if (labelIndex != 951) {
                        Log.w(TAG, "The raw image is not orange!");
                        isFailed = true;
                    }
                }
            });

            Log.d(TAG, "Current state is " + pipe.getState());

            /* start pipeline */
            pipe.start();

            /* push input buffer */
            TensorsData in = readRawImageData();
            if (in != null) {
                pipe.inputData("srcx", in);
            }

            /* wait for classification result */
            Thread.sleep(1000);

            pipe.close();
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, e.getMessage());
            isFailed = true;
        }
    }

    /**
     * Example to run pipeline with valve.
     */
    private void runPipeValve() {
        try {
            String desc = "appsrc name=srcx ! " +
                    "other/tensor,dimension=(string)3:100:100:1,type=(string)uint8,framerate=(fraction)0/1 ! " +
                    "tee name=t " +
                    "t. ! queue ! tensor_sink name=sink1 " +
                    "t. ! queue ! valve name=valvex ! tensor_sink name=sink2";

            Pipeline pipe = new Pipeline(desc);

            /* register sink callback */
            pipe.registerSinkCallback("sink1", new Pipeline.NewDataCallback() {
                int received = 0;

                @Override
                public void onNewDataReceived(TensorsData data) {
                    Log.d(TAG, "Received new data callback at sink1 " + (++received));

                    printTensorsInfo(data.getTensorsInfo());
                    printTensorsData(data);
                }
            });

            pipe.registerSinkCallback("sink2", new Pipeline.NewDataCallback() {
                int received = 0;

                @Override
                public void onNewDataReceived(TensorsData data) {
                    Log.d(TAG, "Received new data callback at sink2 " + (++received));

                    printTensorsInfo(data.getTensorsInfo());
                    printTensorsData(data);
                }
            });

            /* start pipeline */
            pipe.start();

            /* push input buffer */
            TensorsInfo info = new TensorsInfo();
            info.addTensorInfo(NNStreamer.TensorType.UINT8, new int[]{3,100,100,1});

            for (int i = 0; i < 15; i++) {
                /* dummy input */
                TensorsData in = info.allocate();

                Log.d(TAG, "Push input data " + (i + 1));

                pipe.inputData("srcx", in);
                Thread.sleep(50);

                if (i == 10) {
                    /* close valve */
                    pipe.controlValve("valvex", false);
                }
            }

            pipe.close();
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, e.getMessage());
            isFailed = true;
        }
    }

    /**
     * Example to run pipeline with output-selector.
     */
    private void runPipeSwitch() {
        try {
            /* Note that the sink element needs option 'async=false'
             *
             * Prerolling problem
             * For running the pipeline, set async=false in the sink element when using an output selector.
             * The pipeline state can be changed to paused after all sink element receive buffer.
             */
            String desc = "appsrc name=srcx ! " +
                    "other/tensor,dimension=(string)3:100:100:1,type=(string)uint8,framerate=(fraction)0/1 ! " +
                    "output-selector name=outs " +
                    "outs.src_0 ! tensor_sink name=sink1 async=false " +
                    "outs.src_1 ! tensor_sink name=sink2 async=false";

            Pipeline pipe = new Pipeline(desc);

            /* register sink callback */
            pipe.registerSinkCallback("sink1", new Pipeline.NewDataCallback() {
                int received = 0;

                @Override
                public void onNewDataReceived(TensorsData data) {
                    Log.d(TAG, "Received new data callback at sink1 " + (++received));

                    printTensorsInfo(data.getTensorsInfo());
                    printTensorsData(data);
                }
            });

            pipe.registerSinkCallback("sink2", new Pipeline.NewDataCallback() {
                int received = 0;

                @Override
                public void onNewDataReceived(TensorsData data) {
                    Log.d(TAG, "Received new data callback at sink2 " + (++received));

                    printTensorsInfo(data.getTensorsInfo());
                    printTensorsData(data);
                }
            });

            /* start pipeline */
            pipe.start();

            /* push input buffer */
            TensorsInfo info = new TensorsInfo();
            info.addTensorInfo(NNStreamer.TensorType.UINT8, new int[]{3,100,100,1});

            for (int i = 0; i < 15; i++) {
                /* dummy input */
                TensorsData in = TensorsData.allocate(info);

                Log.d(TAG, "Push input data " + (i + 1));

                pipe.inputData("srcx", in);
                Thread.sleep(50);

                if (i == 10) {
                    /* select pad */
                    pipe.selectSwitchPad("outs", "src_1");
                }
            }

            /* get pad list of output-selector */
            String[] pads = pipe.getSwitchPads("outs");
            Log.d(TAG, "Total pad in output-selector: " + pads.length);
            for (String pad : pads) {
                Log.d(TAG, "Pad name: " + pad);
            }

            pipe.close();
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, e.getMessage());
            isFailed = true;
        }
    }

    /**
     * Example to run pipeline with custom filter.
     */
    private void runPipeCustomFilter() {
        try {
            TensorsInfo inInfo = new TensorsInfo();
            inInfo.addTensorInfo(NNStreamer.TensorType.INT32, new int[]{10});

            TensorsInfo outInfo = inInfo.clone();

            /* register custom-filter (passthrough) */
            CustomFilter customPassthrough = CustomFilter.create("custom-passthrough",
                    inInfo, outInfo, new CustomFilter.Callback() {
                @Override
                public TensorsData invoke(TensorsData in) {
                    Log.d(TAG, "Received invoke callback in custom-passthrough");
                    return in;
                }
            });

            /* register custom-filter (convert data type to float) */
            outInfo.setTensorType(0, NNStreamer.TensorType.FLOAT32);
            CustomFilter customConvert = CustomFilter.create("custom-convert",
                    inInfo, outInfo, new CustomFilter.Callback() {
                @Override
                public TensorsData invoke(TensorsData in) {
                    Log.d(TAG, "Received invoke callback in custom-convert");

                    TensorsInfo info = in.getTensorsInfo();
                    ByteBuffer input = in.getTensorData(0);

                    info.setTensorType(0, NNStreamer.TensorType.FLOAT32);

                    TensorsData out = info.allocate();
                    ByteBuffer output = out.getTensorData(0);

                    for (int i = 0; i < 10; i++) {
                        float value = (float) input.getInt(i * 4);
                        output.putFloat(i * 4, value);
                    }

                    out.setTensorData(0, output);
                    return out;
                }
            });

            /* register custom-filter (add constant) */
            inInfo.setTensorType(0, NNStreamer.TensorType.FLOAT32);
            CustomFilter customAdd = CustomFilter.create("custom-add",
                    inInfo, outInfo, new CustomFilter.Callback() {
                @Override
                public TensorsData invoke(TensorsData in) {
                    Log.d(TAG, "Received invoke callback in custom-add");

                    TensorsInfo info = in.getTensorsInfo();
                    ByteBuffer input = in.getTensorData(0);

                    TensorsData out = info.allocate();
                    ByteBuffer output = out.getTensorData(0);

                    for (int i = 0; i < 10; i++) {
                        float value = input.getFloat(i * 4);

                        /* add constant */
                        value += 1.5;
                        output.putFloat(i * 4, value);
                    }

                    out.setTensorData(0, output);
                    return out;
                }
            });

            String desc = "appsrc name=srcx ! " +
                    "other/tensor,dimension=(string)10:1:1:1,type=(string)int32,framerate=(fraction)0/1 ! " +
                    "tensor_filter framework=custom-easy model=" + customPassthrough.getName() + " ! " +
                    "tensor_filter framework=custom-easy model=" + customConvert.getName() + " ! " +
                    "tensor_filter framework=custom-easy model=" + customAdd.getName() + " ! " +
                    "tensor_sink name=sinkx";

            Pipeline pipe = new Pipeline(desc);

            /* register sink callback */
            pipe.registerSinkCallback("sinkx", new Pipeline.NewDataCallback() {
                int received = 0;

                @Override
                public void onNewDataReceived(TensorsData data) {
                    Log.d(TAG, "Received new data callback at sinkx " + (++received));

                    printTensorsInfo(data.getTensorsInfo());
                    printTensorsData(data);

                    ByteBuffer output = data.getTensorData(0);

                    for (int i = 0; i < 10; i++) {
                        Log.d(TAG, "Received data: index " + i + " value " + output.getFloat(i * 4));
                    }
                }
            });

            /* start pipeline */
            pipe.start();

            /* push input buffer */
            TensorsInfo info = new TensorsInfo();
            info.addTensorInfo(NNStreamer.TensorType.INT32, new int[]{10,1,1,1});

            for (int i = 0; i < 15; i++) {
                TensorsData in = info.allocate();
                ByteBuffer input = in.getTensorData(0);

                for (int j = 0; j < 10; j++) {
                    input.putInt(j * 4, j);
                }

                in.setTensorData(0, input);

                pipe.inputData("srcx", in);
                Thread.sleep(50);
            }

            pipe.close();

            /* close custom-filter */
            customPassthrough.close();
            customConvert.close();
            customAdd.close();
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, e.getMessage());
            isFailed = true;
        }
    }

    /**
     * Example to run pipeline with NNFW model.
     */
    private void runPipeNNFW() {
        if (!NNStreamer.isAvailable(NNStreamer.NNFWType.NNFW)) {
            /* cannot run the example */
            Log.w(TAG, "NNFW is not available, cannot run the example.");
            return;
        }

        File model = getExampleModelNNFW();

        if (model == null || !model.exists()) {
            Log.w(TAG, "Cannot find the model file");
            return;
        }

        String desc = "appsrc name=srcx ! " +
                "other/tensor,dimension=(string)1:1:1:1,type=(string)float32,framerate=(fraction)0/1 ! " +
                "tensor_filter framework=nnfw model=" + model.getAbsolutePath() + " ! " +
                "tensor_sink name=sinkx";

        try (Pipeline pipe = new Pipeline(desc)) {
            TensorsInfo info = new TensorsInfo();
            info.addTensorInfo(NNStreamer.TensorType.FLOAT32, new int[]{1,1,1,1});

            /* register sink callback */
            pipe.registerSinkCallback("sinkx", new Pipeline.NewDataCallback() {
                int received = 0;

                @Override
                public void onNewDataReceived(TensorsData data) {
                    float expected = received + 3.5f;

                    Log.d(TAG, "Received new data callback at sinkx " + (++received));

                    printTensorsInfo(data.getTensorsInfo());
                    printTensorsData(data);

                    ByteBuffer output = data.getTensorData(0);
                    float value = output.getFloat(0);

                    /* check output */
                    if (value != expected) {
                        Log.d(TAG, "Failed, received data is invalid.");
                        isFailed = true;
                    }
                }
            });

            /* start pipeline */
            pipe.start();

            /* push input buffer */
            for (int i = 0; i < 5; i++) {
                /* input data */
                TensorsData input = info.allocate();

                ByteBuffer buffer = input.getTensorData(0);
                buffer.putFloat(0, i + 1.5f);

                input.setTensorData(0, buffer);

                /* push data */
                pipe.inputData("srcx", input);

                Thread.sleep(50);
            }

            pipe.stop();
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, e.getMessage());
            isFailed = true;
        }
    }

    /**
     * Example to run single-shot with NNFW model.
     */
    private void runSingleNNFW() {
        if (!NNStreamer.isAvailable(NNStreamer.NNFWType.NNFW)) {
            /* cannot run the example */
            Log.w(TAG, "NNFW is not available, cannot run the example.");
            return;
        }

        try {
            File model = getExampleModelNNFW();

            SingleShot single = new SingleShot(model, NNStreamer.NNFWType.NNFW);

            /* set timeout (1 second) */
            single.setTimeout(1000);

            /* get input info */
            TensorsInfo in = single.getInputInfo();

            /* single-shot invoke */
            for (int i = 0; i < 5; i++) {
                /* input data */
                TensorsData input = in.allocate();

                ByteBuffer buffer = input.getTensorData(0);
                buffer.putFloat(0, i + 1.5f);

                input.setTensorData(0, buffer);

                /* invoke */
                TensorsData output = single.invoke(input);

                printTensorsInfo(output.getTensorsInfo());
                printTensorsData(output);

                /* check output */
                float expected = i + 3.5f;
                if (output.getTensorData(0).getFloat(0) != expected) {
                    Log.d(TAG, "Failed, received data is invalid.");
                    isFailed = true;
                }

                Thread.sleep(30);
            }

            single.close();
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, e.getMessage());
            isFailed = true;
        }
    }
}
