#include "STTModel.h"

#include "whisper.h"

#include <chrono>
#include <iostream>
#include <thread>

// Fake STT implementation useful while testing the rest of the voice pipeline.
std::string MockSTT::transcribe(const std::vector<int16_t>& audioData, int sampleRate) {
    std::cout << "\n[STT Layer] Processing " << audioData.size() << " samples...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return "Hello, I am testing the voice assistant.";
}

// Load the Whisper model once at startup so later transcriptions can reuse it.
LocalWhisperSTT::LocalWhisperSTT(const std::string& modelPath) {
    ctx = whisper_init_from_file(modelPath.c_str());
    if (ctx == nullptr) {
        std::cerr << "Failed to initialize Whisper model from " << modelPath << "\n";
        std::exit(1);
    }
    std::cout << "[STT] Whisper model loaded successfully.\n";
}

LocalWhisperSTT::~LocalWhisperSTT() {
    if (ctx) {
        whisper_free(ctx);
    }
}

// Run Whisper over one complete utterance captured by the audio engine.
std::string LocalWhisperSTT::transcribe(const std::vector<int16_t>& audioData, int sampleRate) {
    if (audioData.empty()) return "";

    std::cout << "\n[STT] Running local Whisper inference...\n";

    // Whisper expects floating-point samples in the range [-1.0, 1.0].
    std::vector<float> pcmf32(audioData.size());
    for (size_t i = 0; i < audioData.size(); i++) {
        pcmf32[i] = (float)audioData[i] / 32768.0f;
    }

    // Configure inference for English transcription with minimal console output.
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress   = false;
    params.print_realtime   = false;
    params.print_timestamps = false;
    params.language         = "en";
    params.n_threads        = 4;

    // Ask Whisper to transcribe the full buffered utterance.
    if (whisper_full(ctx, params, pcmf32.data(), pcmf32.size()) != 0) {
        std::cerr << "Whisper inference failed\n";
        return "";
    }

    // Whisper returns segmented text; join those pieces into one final string.
    std::string result = "";
    const int nSegments = whisper_full_n_segments(ctx);
    for (int i = 0; i < nSegments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        result += text;
    }

    return result;
}
