xpanes -c "{}" \
    './build/bin/stream-with-finality -m ./models/ggml-tiny.en.bin --udp-target-addr 127.0.0.1 --confirmed-tokens-port 42000 --raw-port 42001 --giovanni-prompt-port 42010  --step 250 --length 10000 -c 1' \
    'socat -u UDP-RECV:42000 -' \
    'socat -u UDP-RECV:42001 -' \
    'socat -u UDP-RECV:42010 -'
