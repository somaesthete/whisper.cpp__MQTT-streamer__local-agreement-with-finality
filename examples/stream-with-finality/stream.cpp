// Real-time speech recognition of input from a microphone
//
// A very quick-n-dirty implementation serving mainly as a proof of concept.
//
#include "common-sdl.h"
#include "common.h"
#include "whisper.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <array>
#include <sstream>
#include <numeric>
#include <memory>
#include <mqtt/async_client.h>
#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2tcpip.h>
#else
#include <unistd.h>
#endif

#include "driver.hpp"
#include "simpleTcpDebug.hpp"

using namespace std;

//  500 -> 00:05.000
// 6000 -> 01:00.000
string to_timestamp(int64_t t) {
    int64_t sec = t/100;
    int64_t msec = t - sec*100;
    int64_t min = sec/60;
    sec = sec - min*60;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d.%03d", (int) min, (int) sec, (int) msec);

    return string(buf);
}

// command-line parameters
struct whisper_params {
    int32_t n_threads  = min(4, (int32_t) thread::hardware_concurrency());
    int32_t step_ms    = 3000;
    int32_t length_ms  = 10000;
    int32_t keep_ms    = 200;
    int32_t capture_id = -1;
    int32_t max_tokens = 32;
    int32_t audio_ctx  = 0;

    float vad_thold    = 0.6f;
    float freq_thold   = 100.0f;

    bool speed_up      = false;
    bool translate     = false;
    bool no_fallback   = false;
    bool print_special = false;
    bool no_context    = true;
    bool no_timestamps = false;
    bool tinydiarize   = false;
    bool save_audio    = false; // save audio to wav file
    bool use_gpu       = true;

    string language  = "en";
    string model     = "models/ggml-base.en.bin";
    string fname_out;

    // MQTT params
    string mqtt_broker = "tcp://127.0.0.1:1883";
    string mqtt_client_id = "whisper_stream";
    bool use_mqtt = true;

    // UDP socket stream params
    int32_t confirmed_tokens_port = 42000;
    int32_t raw_inference_frame_port = 42001;
    int32_t giovanni_prompt_port = 42010;

    string udp_target_addr = "127.0.0.1";
};

void whisper_print_usage(int argc, char ** argv, const whisper_params & params);

bool whisper_params_parse(int argc, char ** argv, whisper_params & params) {
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        cout << "=======================================" << endl;
        cout << "parsing " << arg << " " << argv[i] << " " << argv[i + 1] << endl;

        if (arg == "-h" || arg == "--help") {
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
        else if (arg == "-t"    || arg == "--threads")       { params.n_threads     = stoi(argv[++i]); }
        else if (                  arg == "--step")          { params.step_ms       = stoi(argv[++i]); }
        else if (                  arg == "--length")        { params.length_ms     = stoi(argv[++i]); }
        else if (                  arg == "--keep")          { params.keep_ms       = stoi(argv[++i]); }
        else if (arg == "-c"    || arg == "--capture")       { params.capture_id    = stoi(argv[++i]); }
        else if (arg == "-mt"   || arg == "--max-tokens")    { params.max_tokens    = stoi(argv[++i]); }
        else if (arg == "-ac"   || arg == "--audio-ctx")     { params.audio_ctx     = stoi(argv[++i]); }
        else if (arg == "-vth"  || arg == "--vad-thold")     { params.vad_thold     = stof(argv[++i]); }
        else if (arg == "-fth"  || arg == "--freq-thold")    { params.freq_thold    = stof(argv[++i]); }
        else if (arg == "-su"   || arg == "--speed-up")      { params.speed_up      = true; }
        else if (arg == "-tr"   || arg == "--translate")     { params.translate     = true; }
        else if (arg == "-nf"   || arg == "--no-fallback")   { params.no_fallback   = true; }
        else if (arg == "-ps"   || arg == "--print-special") { params.print_special = true; }
        else if (arg == "-kc"   || arg == "--keep-context")  { params.no_context    = false; }
        else if (arg == "-l"    || arg == "--language")      { params.language      = argv[++i]; }
        else if (arg == "-m"    || arg == "--model")         { params.model         = argv[++i]; }
        else if (arg == "-f"    || arg == "--file")          { params.fname_out     = argv[++i]; }
        else if (arg == "-tdrz" || arg == "--tinydiarize")   { params.tinydiarize   = true; }
        else if (arg == "-sa"   || arg == "--save-audio")    { params.save_audio    = true; }
        else if (arg == "-ng"   || arg == "--no-gpu")        { params.use_gpu       = false; }

        else if (                  arg == "--raw-port")                   { params.raw_inference_frame_port = stoi(argv[++i]); }
        else if (                  arg == "--confirmed-tokens-port")      { params.confirmed_tokens_port = stoi(argv[++i]); }
        else if (                  arg == "--giovanni-prompt-port")       { params.giovanni_prompt_port = stoi(argv[++i]); }
        else if (                  arg == "--udp-target-addr"    )        { params.udp_target_addr = argv[++i]; }

        else if (arg == "--mqtt-broker") { params.mqtt_broker = argv[++i]; }
        else if (arg == "--mqtt-client-id") { params.mqtt_client_id = argv[++i]; }
        else if (arg == "--no-mqtt") { params.use_mqtt = false; }

        else {
            fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
            whisper_print_usage(argc, argv, params);
            exit(0);
        }
    }

    return true;
}

void whisper_print_usage(int /*argc*/, char ** argv, const whisper_params & params) {
    fprintf(stderr, "\n");
    fprintf(stderr, "usage: %s [options]\n", argv[0]);
    fprintf(stderr, "\n");
    fprintf(stderr, "options:\n");
    fprintf(stderr, "  -h,       --help          [default] show this help message and exit\n");
    fprintf(stderr, "  -t N,     --threads N     [%-7d] number of threads to use during computation\n",    params.n_threads);
    fprintf(stderr, "            --step N        [%-7d] audio step size in milliseconds\n",                params.step_ms);
    fprintf(stderr, "            --length N      [%-7d] audio length in milliseconds\n",                   params.length_ms);
    fprintf(stderr, "            --keep N        [%-7d] audio to keep from previous step in ms\n",         params.keep_ms);
    fprintf(stderr, "  -c ID,    --capture ID    [%-7d] capture device ID\n",                              params.capture_id);
    fprintf(stderr, "  -mt N,    --max-tokens N  [%-7d] maximum number of tokens per audio chunk\n",       params.max_tokens);
    fprintf(stderr, "  -ac N,    --audio-ctx N   [%-7d] audio context size (0 - all)\n",                   params.audio_ctx);
    fprintf(stderr, "  -vth N,   --vad-thold N   [%-7.2f] voice activity detection threshold\n",           params.vad_thold);
    fprintf(stderr, "  -fth N,   --freq-thold N  [%-7.2f] high-pass frequency cutoff\n",                   params.freq_thold);
    fprintf(stderr, "  -su,      --speed-up      [%-7s] speed up audio by x2 (reduced accuracy)\n",        params.speed_up ? "true" : "false");
    fprintf(stderr, "  -tr,      --translate     [%-7s] translate from source language to english\n",      params.translate ? "true" : "false");
    fprintf(stderr, "  -nf,      --no-fallback   [%-7s] do not use temperature fallback while decoding\n", params.no_fallback ? "true" : "false");
    fprintf(stderr, "  -ps,      --print-special [%-7s] print special tokens\n",                           params.print_special ? "true" : "false");
    fprintf(stderr, "  -kc,      --keep-context  [%-7s] keep context between audio chunks\n",              params.no_context ? "false" : "true");
    fprintf(stderr, "  -l LANG,  --language LANG [%-7s] spoken language\n",                                params.language.c_str());
    fprintf(stderr, "  -m FNAME, --model FNAME   [%-7s] model path\n",                                     params.model.c_str());
    fprintf(stderr, "  -f FNAME, --file FNAME    [%-7s] text output file name\n",                          params.fname_out.c_str());
    fprintf(stderr, "  -tdrz,    --tinydiarize   [%-7s] enable tinydiarize (requires a tdrz model)\n",     params.tinydiarize ? "true" : "false");
    fprintf(stderr, "  -sa,      --save-audio    [%-7s] save the recorded audio to a file\n",              params.save_audio ? "true" : "false");
    fprintf(stderr, "  -ng,      --no-gpu        [%-7s] disable GPU inference\n",                          params.use_gpu ? "false" : "true");
    fprintf(stderr, "            --udp-target-addr        [%-7s] target addr for UDP msg\n",                        params.udp_target_addr.c_str());
    fprintf(stderr, "  --mqtt-broker ADDR:PORT    MQTT broker address (default: %s)\n", params.mqtt_broker.c_str());
    fprintf(stderr, "  --mqtt-client-id ID        MQTT client ID (default: %s)\n", params.mqtt_client_id.c_str());
    fprintf(stderr, "  --no-mqtt                  Disable MQTT publishing\n");
    fprintf(stderr, "\n");
}

// MQTT client wrapper class
class MQTTClient {
private:
    mqtt::async_client client;
    mqtt::connect_options connOpts;
    bool connected = false;

public:
    MQTTClient(const string& broker, const string& clientId) 
        : client(broker, clientId) {
        connOpts.set_clean_session(true);
    }

    bool connect() {
        try {
            client.connect(connOpts)->wait();
            connected = true;
            return true;
        } catch (const mqtt::exception& exc) {
            cerr << "MQTT connection error: " << exc.what() << endl;
            return false;
        }
    }

    void publish(const string& topic, const string& payload) {
        if (!connected) return;
        try {
            client.publish(topic, payload.c_str(), payload.length(), 0, false)->wait();
        } catch (const mqtt::exception& exc) {
            cerr << "MQTT publish error: " << exc.what() << endl;
        }
    }

    ~MQTTClient() {
        if (connected) {
            try {
                client.disconnect()->wait();
            } catch (const mqtt::exception& exc) {
                cerr << "MQTT disconnect error: " << exc.what() << endl;
            }
        }
    }
};

int main(int argc, char ** argv) {
    whisper_params params;

    if (whisper_params_parse(argc, argv, params) == false) {
        return 1;
    }

    params.keep_ms   = min(params.keep_ms,   params.step_ms);
    params.length_ms = max(params.length_ms, params.step_ms);

    const int n_samples_step = (1e-3*params.step_ms  )*WHISPER_SAMPLE_RATE;
    const int n_samples_len  = (1e-3*params.length_ms)*WHISPER_SAMPLE_RATE;
    const int n_samples_keep = (1e-3*params.keep_ms  )*WHISPER_SAMPLE_RATE;
    const int n_samples_30s  = (1e-3*30000.0         )*WHISPER_SAMPLE_RATE;

    const bool use_vad = n_samples_step <= 0; // sliding window mode uses VAD

    const int n_new_line = !use_vad ? max(1, params.length_ms / params.step_ms - 1) : 1; // number of steps to print new line

    params.no_timestamps  = !use_vad;
    // params.no_timestamps = false;

    params.no_context    |= use_vad;
    params.max_tokens     = 0;

    // init audio

    audio_async audio(params.length_ms);
    if (!audio.init(params.capture_id, WHISPER_SAMPLE_RATE)) {
        fprintf(stderr, "%s: audio.init() failed!\n", __func__);
        return 1;
    }

    audio.resume();

    // whisper init
    if (params.language != "auto" && whisper_lang_id(params.language.c_str()) == -1){
        fprintf(stderr, "error: unknown language '%s'\n", params.language.c_str());
        whisper_print_usage(argc, argv, params);
        exit(0);
    }

    struct whisper_context_params cparams;
    cparams.use_gpu = params.use_gpu;

    struct whisper_context * ctx = whisper_init_from_file_with_params(params.model.c_str(), cparams);

    vector<float> pcmf32    (n_samples_30s, 0.0f);
    vector<float> pcmf32_old;
    vector<float> pcmf32_new(n_samples_30s, 0.0f);

    vector<whisper_token> prompt_tokens;

    // print some info about the processing
    {
        fprintf(stderr, "\n");
        if (!whisper_is_multilingual(ctx)) {
            if (params.language != "en" || params.translate) {
                params.language = "en";
                params.translate = false;
                fprintf(stderr, "%s: WARNING: model is not multilingual, ignoring language and translation options\n", __func__);
            }
        }
        fprintf(stderr, "%s: processing %d samples (step = %.1f sec / len = %.1f sec / keep = %.1f sec), %d threads, lang = %s, task = %s, timestamps = %d ...\n",
                __func__,
                n_samples_step,
                float(n_samples_step)/WHISPER_SAMPLE_RATE,
                float(n_samples_len )/WHISPER_SAMPLE_RATE,
                float(n_samples_keep)/WHISPER_SAMPLE_RATE,
                params.n_threads,
                params.language.c_str(),
                params.translate ? "translate" : "transcribe",
                params.no_timestamps ? 0 : 1);

        if (!use_vad) {
            fprintf(stderr, "%s: n_new_line = %d, no_context = %d\n", __func__, n_new_line, params.no_context);
        } else {
            fprintf(stderr, "%s: using VAD, will transcribe on speech activity\n", __func__);
        }

        fprintf(stderr, "\n");
    }

    driver driverInst(3, 2, 100);

    int n_iter = 0;

    // Define the attention span duration in milliseconds
    const int ATTENTION_SPAN_MS = 2700; // Example value, adjust as needed
    // Track whether we are in a listening state
    bool listeningState = false;
    // Timestamp of the last time "giovanni" was mentioned
    auto last_giovanni_mention = chrono::steady_clock::now();
    // Vector to store tokens while in the listening state
    std::vector<std::string> giovanni_tokens;
    // Port to send the concatenated tokens to
    const int GIOVANI_PROMPT_PORT = 42010;
    bool is_running = true;

    // Initialize MQTT client if enabled
    unique_ptr<MQTTClient> mqttClient;
    if (params.use_mqtt) {
        mqttClient.reset(new MQTTClient(params.mqtt_broker, params.mqtt_client_id));
        if (!mqttClient->connect()) {
            fprintf(stderr, "Failed to connect to MQTT broker, continuing without MQTT\n");
            params.use_mqtt = false;
        }
    }

    ofstream fout;
    if (params.fname_out.length() > 0) {
        fout.open(params.fname_out);
        if (!fout.is_open()) {
            fprintf(stderr, "%s: failed to open output file '%s'!\n", __func__, params.fname_out.c_str());
            return 1;
        }
    }

    wav_writer wavWriter;
    // save wav file
    if (params.save_audio) {
        // Get current date/time for filename
        time_t now = time(0);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y%m%d%H%M%S", localtime(&now));
        string filename = string(buffer) + ".wav";

        wavWriter.open(filename, WHISPER_SAMPLE_RATE, 16, 1);
    }
    printf("[Start speaking]\n");
    fflush(stdout);

    auto t_last  = chrono::high_resolution_clock::now();
    const auto t_start = t_last;

    auto last_fetch_time = chrono::steady_clock::now();

    // main audio loop
    while (is_running) {
        if (params.save_audio) {
            wavWriter.write(pcmf32_new.data(), pcmf32_new.size());
        }
        // handle Ctrl + C
        is_running = sdl_poll_events();

        if (!is_running) {
            break;
        }

        // process new audio

        // if (!use_vad) {
        //     while (true) {
        //         audio.get(params.step_ms, pcmf32_new);

        //         if ((int) pcmf32_new.size() > 2*n_samples_step) {
        //             fprintf(stderr, "\n\n%s: WARNING: cannot process audio fast enough, dropping audio ...\n\n", __func__);
        //             audio.clear();
        //             continue;
        //         }

        //         if ((int) pcmf32_new.size() >= n_samples_step) {
        //             audio.clear();
        //             break;
        //         }

        //         this_thread::sleep_for(chrono::milliseconds(1));
        //     }

        //     const int n_samples_new = pcmf32_new.size();

        //     // take up to params.length_ms audio from previous iteration
        //     const int n_samples_take = min((int) pcmf32_old.size(), max(0, n_samples_keep + n_samples_len - n_samples_new));

        //     //printf("processing: take = %d, new = %d, old = %d\n", n_samples_take, n_samples_new, (int) pcmf32_old.size());

        //     pcmf32.resize(n_samples_new + n_samples_take);

        //     for (int i = 0; i < n_samples_take; i++) {
        //         pcmf32[i] = pcmf32_old[pcmf32_old.size() - n_samples_take + i];
        //     }

        //     memcpy(pcmf32.data() + n_samples_take, pcmf32_new.data(), n_samples_new*sizeof(float));

        //     pcmf32_old = pcmf32;
        // } else {
        //     const auto t_now  = chrono::high_resolution_clock::now();
        //     const auto t_diff = chrono::duration_cast<chrono::milliseconds>(t_now - t_last).count();

        //     if (t_diff < 2000) {
        //         this_thread::sleep_for(chrono::milliseconds(100));

        //         continue;
        //     }

        //     audio.get(2000, pcmf32_new);

        //     if (::vad_simple(pcmf32_new, WHISPER_SAMPLE_RATE, 1000, params.vad_thold, params.freq_thold, false)) {
        //         audio.get(params.length_ms, pcmf32);
        //     } else {
        //         this_thread::sleep_for(chrono::milliseconds(100));

        //         continue;
        //     }

        //     t_last = t_now;
        // }

                // Current time
        auto current_time = chrono::steady_clock::now();

        // Calculate elapsed time since the last fetch
        auto elapsed_time = chrono::duration_cast<chrono::milliseconds>(current_time - last_fetch_time);

        // Check if step_ms milliseconds have passed since the last fetch
        if (elapsed_time.count() >= params.step_ms) {
            // Fetch audio data
            audio.get(params.length_ms, pcmf32);

            // Update the last fetch time
            last_fetch_time = current_time;

            // ... rest of the processing ...
        } else {
            // Not enough time has passed, continue and wait
            this_thread::sleep_for(chrono::milliseconds(1));
            continue;
        }

        // run the inference
        {
            whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

            wparams.print_progress   = false;
            wparams.print_special    = params.print_special;
            wparams.print_realtime   = false;
            wparams.print_timestamps = !params.no_timestamps;
            wparams.translate        = params.translate;
            wparams.single_segment   = !use_vad;
            wparams.max_tokens       = params.max_tokens;
            wparams.language         = params.language.c_str();
            wparams.n_threads        = params.n_threads;

            wparams.audio_ctx        = params.audio_ctx;
            // wparams.speed_up         = params.speed_up;

            wparams.tdrz_enable      = params.tinydiarize; // [TDRZ]

            // disable temperature fallback
            //wparams.temperature_inc  = -1.0f;
            wparams.temperature_inc  = params.no_fallback ? 0.0f : wparams.temperature_inc;

            wparams.prompt_tokens    = params.no_context ? nullptr : prompt_tokens.data();
            wparams.prompt_n_tokens  = params.no_context ? 0       : prompt_tokens.size();

            if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
                fprintf(stderr, "%s: failed to process audio\n", argv[0]);
                return 6;
            }

            // receive transcription inference results and process them.

            // macros
            const int GIOVANNI_PROMPT_PORT = 42010;

            // Define the attention span duration in milliseconds
            const int ATTENTION_SPAN_MS = 5000; // adjust as needed

            // Track whether we are in a listening state
            bool listeningState = false;

            // Timestamp of the last time "giovanni" was mentioned
            auto last_giovanni_mention = chrono::steady_clock::now();
            {

                const int n_segments = whisper_full_n_segments(ctx);
                for (int i = 0; i < n_segments; ++i) {
                    const char * text = whisper_full_get_segment_text(ctx, i);

                    // send raw inference to the raw inference port (default 42001)
                    sendMessageToPort(params.udp_target_addr.c_str(), params.raw_inference_frame_port, text);
                    
                    // Publish raw inference to MQTT
                    if (params.use_mqtt) {
                        mqttClient->publish("whisper/raw_inference", text);
                    }

                    // auto [newTokens, ctxBuffer, committed_tokens] = driverInst.drive(text);

                    // without structured bindings.
                    auto resultTuple = driverInst.drive(text);
                    auto newTokens = std::get<0>(resultTuple);
                    auto ctxBuffer = std::get<1>(resultTuple);
                    auto committed_tokens = std::get<2>(resultTuple);

                    if (!newTokens.empty()) {
                        // Check if any token contains the word "giovanni"
                        bool contains_giovanni = std::any_of(newTokens.begin(), newTokens.end(), [](const std::string& token) {
                            return token.find("giovanni") != std::string::npos;
                        });

                        // Enter listening state if "giovanni" is mentioned
                        if (contains_giovanni) {
                            listeningState = true;
                            last_giovanni_mention = chrono::steady_clock::now();
                        }

                        // Store tokens if in listening state
                        if (listeningState) {
                            giovanni_tokens.insert(giovanni_tokens.end(), newTokens.begin(), newTokens.end());
                        }

                        // ANSI escape code for green text
                        const std::string green_text_start = listeningState ? "\033[32m" : "";
                        const std::string green_text_end = listeningState ? "\033[0m" : "";

                        // Get current datetime
                        auto now = chrono::system_clock::now();
                        auto now_c = chrono::system_clock::to_time_t(now);
                        std::tm now_tm = *std::localtime(&now_c);
                        std::stringstream datetime_ss;
                        datetime_ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
                        auto ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) % 1000;
                        datetime_ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

                        // Get hostname
                        std::array<char, 256> hostname_buffer{};

                        gethostname(hostname_buffer.data(), hostname_buffer.size());
                        std::string hostname(hostname_buffer.data());

                        // Compose the message
                        std::stringstream message_ss;
                        message_ss << green_text_start << "[" << datetime_ss.str() << " " << hostname << "] ";
                        for (const auto& token : newTokens) {
                            message_ss << token << ' ';
                        }
                        message_ss << green_text_end;

                        // Print the composed message
                        cout << message_ss.str() << endl;

                        // Convert committed_tokens vector to a single string
                        std::string committed_tokens_str = std::accumulate(
                            committed_tokens.begin(), committed_tokens.end(), std::string(),
                            [](const std::string& a, const std::string& b) {
                                return a.empty() ? b : a + " " + b;
                            }
                        );

                        // send confirmed tokens to the confirmed tokens port (default 42000)
                        sendMessageToPort(params.udp_target_addr.c_str(), params.confirmed_tokens_port, committed_tokens_str);
                        
                        // Publish confirmed tokens to MQTT
                        if (params.use_mqtt) {
                            mqttClient->publish("whisper/confirmed_tokens", committed_tokens_str);
                        }
                    } else {
                        // Check if we should exit the listening state due to inactivity
                        auto current_time = chrono::steady_clock::now();
                        if (listeningState && chrono::duration_cast<chrono::milliseconds>(current_time - last_giovanni_mention).count() > ATTENTION_SPAN_MS) {
                            listeningState = false;

                            // Concatenate and send the stored tokens if any
                            if (!giovanni_tokens.empty()) {
                                std::string concatenated_tokens = std::accumulate(
                                    giovanni_tokens.begin(), giovanni_tokens.end(), std::string(),
                                    [](const std::string& a, const std::string& b) {
                                        return a.empty() ? b : a + " " + b;
                                    }
                                );

                                // Send the concatenated string to GIOVANI_PROMPT_PORT (default 42010)
                                sendMessageToPort(params.udp_target_addr.c_str(), params.giovanni_prompt_port, concatenated_tokens);

                                // Publish giovanni prompt to MQTT
                                if (params.use_mqtt) {
                                    mqttClient->publish("whisper/giovanni_prompt", concatenated_tokens);
                                }

                                // Clear the stored tokens
                                giovanni_tokens.clear();
                            }
                        }
                    }
                    // \print__our__tokens
                    
                }
            }
        }
    }

    audio.pause();

    whisper_print_timings(ctx);
    whisper_free(ctx);

    return 0;
}
