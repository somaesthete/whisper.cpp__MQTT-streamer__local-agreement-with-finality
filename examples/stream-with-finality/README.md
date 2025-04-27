# stream-with-finality

## what

stream-with-finality continuously infers on a sliding window of the last LENGTH ms at a cadence of STEP ms


## build

On my mac.. (windows wouldn't have COREML)

    # instantiate
    cmake -B build -DWHISPER_COREML=1 -DWHISPER_BUILD_EXAMPLES=1 -DWHISPER_SDL2=1
    # build
    cmake --build build

## run

    ./build/bin/stream-with-finality -m ./models/ggml-base.bin --confirmed-tokens-port 42000 --raw-port 42001 --giovanni-prompt-port 

If we like we can use xpanes to open a splitpane and watch the UDP streams.

```./scripts/launchXpanesSplit.
# ./scripts/launchXpanesSplit.
xpanes -c "{}" \
    './build/bin/stream-with-finality -m ./models/ggml-base.bin --confirmed-tokens-port 42000 --raw-port 42001 --giovanni-prompt-port 42010  --step 250 --length 10000 -c 1' \
    'socat -u UDP-RECV:42000,bind=0.0.0.0 -' \
    'socat -u UDP-RECV:42001,bind=0.0.0.0 -' \
    'socat -u UDP-RECV:42010,bind=0.0.0.0 -'
```

![./scripts/launchXpanesSplit.sh ](launchXpanesSplit.sh.png)


## output streams

The results are sent to the following ports:

    int32_t confirmed_tokens_port = 42000;
    int32_t raw_inference_frame_port = 42001;
    int32_t giovanni_prompt_port = 42010;


---

# stream

This is a naive example of performing real-time inference on audio from your microphone.
The `stream` tool samples the audio every half a second and runs the transcription continously.
More info is available in [issue #10](https://github.com/ggerganov/whisper.cpp/issues/10).

```java
./stream -m ./models/ggml-base.en.bin -t 8 --step 500 --length 5000
```

https://user-images.githubusercontent.com/1991296/194935793-76afede7-cfa8-48d8-a80f-28ba83be7d09.mp4

## Sliding window mode with VAD

Setting the `--step` argument to `0` enables the sliding window mode:

```java
 ./stream -m ./models/ggml-small.en.bin -t 6 --step 0 --length 30000 -vth 0.6
```

In this mode, the tool will transcribe only after some speech activity is detected. A very
basic VAD detector is used, but in theory a more sophisticated approach can be added. The
`-vth` argument determines the VAD threshold - higher values will make it detect silence more often.
It's best to tune it to the specific use case, but a value around `0.6` should be OK in general.
When silence is detected, it will transcribe the last `--length` milliseconds of audio and output
a transcription block that is suitable for parsing.

## Building

The `stream` tool depends on SDL2 library to capture audio from the microphone. You can build it like this:

```bash
# Install SDL2 on Linux
sudo apt-get install libsdl2-dev

# Install SDL2 on Mac OS
brew install sdl2

make stream
```

Ensure you are at the root of the repo when running `make stream`.  Not within the `examples/stream` dir
as the libraries needed like `common-sdl.h` are located within `examples`.  Attempting to compile within
`examples/steam` means your compiler cannot find them and it gives an error it cannot find the file.

```bash
whisper.cpp/examples/stream$ make stream
g++     stream.cpp   -o stream
stream.cpp:6:10: fatal error: common/sdl.h: No such file or directory
    6 | #include "common/sdl.h"
      |          ^~~~~~~~~~~~~~
compilation terminated.
make: *** [<builtin>: stream] Error 1
```

## Web version

This tool can also run in the browser: [examples/stream.wasm](/examples/stream.wasm)
