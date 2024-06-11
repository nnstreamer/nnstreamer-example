# Example of GStreamer/NNStreamer subtitle sumerizer pipeline using llama.cpp

## Description

This example shows how to use llama.cpp via GStreamer/NNStreamer pipeline in Tizen/RPI4. Users can use their ML model/app as GStreamer/NNStreamer pipeline if they implement their model/app as a [C++ class](https://github.com/nnstreamer/nnstreamer/blob/main/ext/nnstreamer/tensor_filter/tensor_filter_cpp.hh). This example shows how to use the [llama.cpp](https://github.com/ggerganov/llama.cpp) in pipeline as cpp class tensor_filter.

The cpp class wrapping llama.cpp is implemented in https://github.com/yeonykim2/llama.cpp/tree/nnstreamer_llama_subtitle_summarizer.


## Prerequisites

- rpi4 flashed with the latest tizen-headed (64bit) image.
- Tizen GBS tools.

## Build / Install guide

- Build nnstreamer-llama-cpp rpm package
  ```bash
  $ git clone https://github.com/yeonykim2/llama.cpp && cd llama.cpp && git checkout nnstreamer_llama_subtitle_summarizer
  $ gbs build -A aarch64

  # check contents of the RPM file
  $ ls ~/GBS-ROOT/local/repos/tizen/aarch64/RPMS
  > nnstreamer-llama-68m-gguf-1.0.0-0.aarch64.rpm ...

  $ rpm -qlp nnstreamer-llama-68m-gguf-1.0.0-0.aarch64.rpm 
  > /usr/lib/nnstreamer/bin/big_buck_bunny_trailer_480p.webm
  > /usr/lib/nnstreamer/bin/libnnstreamer-llama.so
  > /usr/lib/nnstreamer/bin/models
  > /usr/lib/nnstreamer/bin/subtitles.srt

  $ cp ~/GBS-ROOT/local/repos/tizen/aarch64/RPMS/nnstreamer-llama-68m-gguf-1.0.0-0.aarch64.rpm .

  # extract the compressed files (option)
  $ rpm2cpio nnstreamer-llama-68m-gguf-1.0.0-0.aarch64.rpm | cpio -idmv
  > ./usr/lib/nnstreamer/bin/big_buck_bunny_trailer_480p.webm
  > ./usr/lib/nnstreamer/bin/libnnstreamer-llama.so
  > ./usr/lib/nnstreamer/bin/models
  > ./usr/lib/nnstreamer/bin/subtitles.srt
  ```

- Download the model file `llama-68m-chat-v1.fp16.gguf` [here](https://huggingface.co/afrideva/Llama-68M-Chat-v1-GGUF).
  ```bash
  # install the rpm package
  $ sdb push nnstreamer-llama-68m-gguf-1.0.0-0.aarch64.rpm /root/
  $ sdb shell rpm -ivh /root/nnstreamer-llama-68m-gguf-1.0.0-0.aarch64.rpm

  # install model, sample video and sample srt file
  $ sdb push models/llama-68m-chat-v1.fp16.gguf /usr/lib/nnstreamer/bin/models/
  $ sdb push subtitles.srt /usr/lib/nnstreamer/bin/
  $ sdb push big_buck_bunny_trailer_480p.webm /usr/lib/nnstreamer/bin/
  ```



## Run pipeline in sdb shell

The following GST-launch example makes a `llama.cpp` summarize on the sample subtitle, `subtitles.srt`, with the `llama-68m-chat-v1.fp16.gguf` model.
- `ORC_DEBUG=` suppresses the debug message from gst-orc.
- `LD_LIBRARY_PATH=.` makes `libnnstreamer-llama.so` viable for gstreamer.

``` bash
# sdb shell
$ cd /usr/lib/nnstreamer/bin/
$ ORC_DEBUG= LD_LIBRARY_PATH=. gst-launch-1.0 -v filesrc location=big_buck_bunny_trailer_480p.webm ! decodebin ! videoconvert ! autovideosink filesrc location=subtitles.srt ! text/x-raw, format=utf8 ! tensor_converter input-dim=48000:1:1:1 ! tensor_filter framework=cpp model=nnstreamer_llama_filter,libnnstreamer-llama.so ! fakesink

> ...
nstreamer_llama_filter: build = 0 (unknown)
nnstreamer_llama_filter: built with clang version 17.0.6 for x86_64-tizen-linux-gnu
nnstreamer_llama_filter: seed  = 10441
nnstreamer_llama_filter: llama backend init
nnstreamer_llama_filter: load the model and apply lora adapter, if any
llama_model_loader: loaded meta data with 21 key-value pairs and 21 tensors from models/llama-68m-chat-v1.fp16.gguf (version GGUF V3 (latest))
llama_model_loader: Dumping metadata keys/values. Note: KV overrides do not apply in this output.
llama_model_loader: - kv   0:                       general.architecture str              = llama
llama_model_loader: - kv   1:                               general.name str              = active
llama_model_loader: - kv   2:                       llama.context_length u32              = 2048
llama_model_loader: - kv   3:                     llama.embedding_length u32              = 768
llama_model_loader: - kv   4:                          llama.block_count u32              = 2
llama_model_loader: - kv   5:                  llama.feed_forward_length u32              = 3072
llama_model_loader: - kv   6:                 llama.rope.dimension_count u32              = 64
llama_model_loader: - kv   7:                 llama.attention.head_count u32              = 12
llama_model_loader: - kv   8:              llama.attention.head_count_kv u32              = 12
llama_model_loader: - kv   9:     llama.attention.layer_norm_rms_epsilon f32              = 0.000001
llama_model_loader: - kv  10:                       llama.rope.freq_base f32              = 10000.000000
llama_model_loader: - kv  11:                          general.file_type u32              = 1
llama_model_loader: - kv  12:                       tokenizer.ggml.model str              = llama

...

prompt: *******

 "<|im_start|>system You are a helpful assistant.<|im_end|> <|im_start|>user Please, summarize the sutitle.
1
00:00:00,005 --> 00:00:03,800
The peach open movie project presents

2
00:00:06,001 --> 00:00:09,001
One big rabbit

3
00:00:10,900 --> 00:00:13,001
Three rodents

4
00:00:16,400 --> 00:00:18,954
And one giant payback

5
00:00:22,950 --> 00:00:25,001
Get ready

6
00:00:26,700 --> 00:00:30,000
Big Buck Bunny

7
00:00:30,001 --> 00:00:31,100
Coming soon

8
00:00:31,101 --> 00:00:32,509
www.bigbuckbunny.org
<|im_end|>"
 ******* 

sampling: 
	repeat_last_n = 64, repeat_penalty = 1.000, frequency_penalty = 0.000, presence_penalty = 0.000
	top_k = 40, tfs_z = 1.000, top_p = 0.950, min_p = 0.050, typical_p = 1.000, temp = 0.800
	mirostat = 0, mirostat_lr = 0.100, mirostat_ent = 5.000
sampling order: 
CFG -> Penalties -> top_k -> tfs_z -> typical_p -> top_p -> min_p -> temperature 
generate: n_ctx = 512, n_batch = 2048, n_predict = 128, n_keep = 1

...

######################
# Generation Results #
######################


<|im_start|>assistant
To get a new job, a film or a video
The most important part of the work is to look at the film's structure, theme, and content. You can start with a good movie title, a short summary of the original film, or a short summary. 
1000<|im_end|>
 [end of text]

Pipeline is PREROLLED ...
Setting pipeline to PLAYING ...
Redistribute latency...
New clock: GstSystemClock
Got EOS from element "pipeline0".
Execution ended after 0:00:32.480586369
Setting pipeline to NULL ...
Freeing pipeline ...


llama_print_timings:        load time =     128.68 ms
llama_print_timings:      sample time =       9.85 ms /    82 runs   (    0.12 ms per token,  8329.10 tokens per second)
llama_print_timings: prompt eval time =    1057.57 ms /   362 tokens (    2.92 ms per token,   342.29 tokens per second)
llama_print_timings:        eval time =    2462.21 ms /    81 runs   (   30.40 ms per token,    32.90 tokens per second)
llama_print_timings:       total time =   36241.92 ms /   443 tokens

  ...
```

- It shows that the llama-68M model can run on the Tizen/RPI4 device.
- Cannot be sure about the performance of LLMs.
