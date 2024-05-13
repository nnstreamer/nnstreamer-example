# Example of GStreamer/NNStreamer pipeline using whisper.cpp

## Description

This example shows how to use whisper.cpp via GStreamer/NNStreamer pipeline in Tizen/RPI4. Users can use their ML model/app as GStreamer/NNStreamer pipeline if they implement their model/app as a [C++ class](https://github.com/nnstreamer/nnstreamer/blob/main/ext/nnstreamer/tensor_filter/tensor_filter_cpp.hh). This example shows how to use the [whisper.cpp](https://github.com/ggerganov/whisper.cpp/) in pipeline as cpp class tensor_filter.

The cpp class wrapping whisper.cpp is implemented in https://github.com/anyj0527/whisper.cpp/tree/nnstreamer-cpp-filter-v1.5.5.

## Prerequisites

- rpi4 flashed with the latest tizen-headed (64bit) image.
- Tizen GBS tools.

## Build / Install guide

- Build nnstreamer-whisper-cpp rpm package
  ```bash
  $ git clone https://github.com/anyj0527/whisper.cpp.git && cd whisper.cpp && git checkout nnstreamer-cpp-filter-v1.5.5
  $ gbs build -A aarch64

  # check contents of the RPM file
  $ ls ~/GBS-ROOT/local/repos/tizen/aarch64/RPMS
  > nnstreamer-whisper-cpp-1.0.0-0.aarch64.rpm ...
  $ cp ~/GBS-ROOT/local/repos/tizen/aarch64/RPMS/nnstreamer-whisper-cpp-1.0.0-0.aarch64.rpm .
  $ rpm2cpio nnstreamer-whisper-cpp-1.0.0-0.aarch64.rpm | cpio -idmv
  > ./usr/lib/nnstreamer/filters/libnnstreamer-whisper.so
  ```

- Download the model file `ggml-tiny.en.bin` and sample audio files
  ```bash
  $ models/download-ggml-model.sh tiny.en
  $ ls models
  > ... ggml-tiny.en.bin ...
  $ make samples # make sure ffmpeg is in your $PATH
  $ ls samples
  > ... gb0.wav gb1.wav jfk.wav ...

  # install the rpm package
  $ sdb push nnstreamer-whisper-cpp-1.0.0-0.aarch64.rpm /root/
  $ sdb shell rpm -ivh /root/nnstreamer-whisper-cpp-1.0.0-0.aarch64.rpm

  # install model and sample audio file
  $ sdb push models/ggml-tiny.en.bin /usr/lib/nnstreamer/bin/models/
  $ sdb push samples/gb0.wav /usr/lib/nnstreamer/bin/
  ```

## Run pipeline in sdb shell

Following gst-launch example make whisper.cpp do STT on the sample `gb0.wav` with ggml-tiny.en model.
- `ORC_DEBUG=` suppresses the debug message from gst-orc.
- `LD_LIBRARY_PATH=.` makes `libnnstreamer-whisper.so` viable for gstreamer.

``` bash
# sdb shell
$ cd /usr/lib/nnstreamer/bin/
$ ORC_DEBUG= LD_LIBRARY_PATH=. gst-launch-1.0 \
filesrc location=gb0.wav ! wavparse ! audioconvert ! audio/x-raw,format=S16LE,channels=1,rate=16000,layout=interleaved ! \
tensor_converter frames-per-tensor=3200 ! tensor_aggregator frames-in=3200 frames-out=48000 frames-flush=44800 frames-dim=1 ! \
tensor_transform mode=arithmetic option=typecast:float32,add:0.5,div:32767.5 ! tensor_transform mode=dimchg option=0:1 ! \
other/tensors,num_tensors=1,dimensions=48000:1:1:1,types=float32,format=static ! \
tensor_filter framework=cpp model=nnstreamer_whisper_filter,libnnstreamer-whisper.so ! \
fakesink
> ...
  whisper_init_from_file_with_params_no_state: loading model from 'models/ggml-tiny.en.bin'
  whisper_model_load: loading model
  whisper_model_load: n_vocab       = 51864
  whisper_model_load: n_audio_ctx   = 1500
  whisper_model_load: n_audio_state = 384
  whisper_model_load: n_audio_head  = 6
  whisper_model_load: n_audio_layer = 4
  whisper_model_load: n_text_ctx    = 448
  whisper_model_load: n_text_state  = 384
  whisper_model_load: n_text_head   = 6
  whisper_model_load: n_text_layer  = 4
  whisper_model_load: n_mels        = 80
  whisper_model_load: ftype         = 1
  whisper_model_load: qntvr         = 0
  whisper_model_load: type          = 1 (tiny)
  whisper_model_load: adding 1607 extra tokens
  whisper_model_load: n_langs       = 99
  whisper_model_load:      CPU total size =    77.11 MB
  whisper_model_load: model size    =   77.11 MB
  whisper_init_state: kv self size  =    8.26 MB
  whisper_init_state: kv cross size =    9.22 MB
  whisper_init_state: compute buffer (conv)   =   13.32 MB
  whisper_init_state: compute buffer (encode) =   85.66 MB
  whisper_init_state: compute buffer (cross)  =    4.01 MB
  whisper_init_state: compute buffer (decode) =   96.02 MB
  Setting pipeline to PAUSED ...
  Pipeline is PREROLLING ...
   Good morning. This Tuesday is Election Day.
  Pipeline is PREROLLED ...
  Setting pipeline to PLAYING ...
  Redistribute latency...
   New clock: GstSystemClock
   After much of spirited debate in vigorous campaigning.
   the time has come for Americans to make important decisions.
   about our nation's future and encourage all Americans to
   to go to the polls and vote.
   and season brings out the spirit of competition between our political
   in that competition is an essential
   a healthy democracy.
   payments come to a close Republicans, Democrats and independents.
   the
   point. Our system of representative democracy
   is one of America's greatest strengths.
   The United States was founded on the belief that all men
   all men are created equal. Every Election Day.
   Millions of Americans of all races, religions, and
   and background step into voting boost throughout the nation.
   Whether they are rich or poor, old or young.
   Each of them has an equal share and choosing the path.
   path that our country will take.
   They cast is a reminder that our founding principles
   will survive and well.
   on a great privileges of American citizenship.
   And it is always required brave defenders.
   As you head to the polls next week, remember the sacrifices
   that had been made by generations of Americans in Europe.
   uniform to preserve our way of life.
   from Bunker Hill to Baghdad. The men and women of America
   American Armed Forces have been devoted guardians of ou democracy.
   democracy. All of us owe them and their fans
   families a special debt of gratitude on election day.
   >> Okay. >> America should also remember the important
   an example that our elections set throughout the world.
   Young democracy from Georgia and Ukraine to Afghanistan.
   and look to the United States for proof.
   the self-government can endure and nations
   the still of under tyranny and oppression can find hope.
   hope and inspiration in our commitment to liberty.
   For more than two centuries, Americans have demonstrated the
   the ability of free people to choose their own leaders.
   Our nation has flourished because of its commitment to trust
   trusting the wisdom of our citizenry.
   year's election. We will see this tradition continue.
   you. And we will be reminded once again that we are
   we are blessed to live in a free nation.
   will of the people. Thank you for listening.
  Got EOS from element "pipeline0".
  Execution ended after 0:11:12.973268303
  Setting pipeline to NULL ...
  Freeing pipeline ...

  whisper_print_timings:     load time =   326.41 ms
  whisper_print_timings:     fallbacks =   0 p /   0 h
  whisper_print_timings:      mel time =  1333.17 ms
  whisper_print_timings:   sample time =  1263.47 ms /     1 runs ( 1263.47 ms per run)
  whisper_print_timings:   encode time = 670238.06 ms /    45 runs (14894.18 ms per run)
  whisper_print_timings:   decode time = 13349.55 ms /   518 runs (   25.77 ms per run)
  whisper_print_timings:   batchd time =     0.00 ms /     1 runs (    0.00 ms per run)
  whisper_print_timings:   prompt time =     0.00 ms /     1 runs (    0.00 ms per run)
  whisper_print_timings:    total time = 686719.25 ms
  ...
```

- It took 11m 13s to transcribe wav file of 2m 7s (quite slow).
- If you replace `filesrc location=gb0.wav ! wavparse` with `alsasrc hw:3` and connect microphone device to the RPI4 device, the pipeline will be able to transcribe the audio stream from the microphone in very slow speed...
