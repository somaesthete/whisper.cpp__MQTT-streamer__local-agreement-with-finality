#!/bin/bash

# Set USE_MQTT to 1 to use MQTT instead of UDP
USE_MQTT=${USE_MQTT:-0}

if [ "$USE_MQTT" = "1" ]; then
    # Use MQTT
    xpanes -c "{}" \
        './build/bin/stream-with-finality -m ./models/ggml-tiny.en.bin --udp-target-addr 127.0.0.1 --confirmed-tokens-port 42000 --raw-port 42001 --giovanni-prompt-port 42010 --step 250 --length 10000 -c 1' \
        'mosquitto_sub -t whisper/confirmed_tokens' \
        'mosquitto_sub -t whisper/raw_inference' \
        'mosquitto_sub -t whisper/giovanni_prompt'
else
    # Use UDP (default)
    xpanes -c "{}" \
        './build/bin/stream-with-finality -m ./models/ggml-tiny.en.bin --udp-target-addr 127.0.0.1 --confirmed-tokens-port 42000 --raw-port 42001 --giovanni-prompt-port 42010 --step 250 --length 10000 -c 1' \
        'socat -u UDP-RECV:42000 -' \
        'socat -u UDP-RECV:42001 -' \
        'socat -u UDP-RECV:42010 -'
fi
