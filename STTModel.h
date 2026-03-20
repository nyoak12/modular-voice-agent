#ifndef STT_MODEL_H
#define STT_MODEL_H

#include <cstdint>
#include <string>
#include <vector>

// Common interface for speech-to-text backends.
// The rest of the app can talk to STT through this API without caring about the implementation.
class STTModel {
public:
    virtual ~STTModel() = default;
    virtual std::string transcribe(const std::vector<int16_t>& audioData, int sampleRate) = 0;
};

// Lightweight test implementation that pretends to transcribe audio.
class MockSTT : public STTModel {
public:
    std::string transcribe(const std::vector<int16_t>& audioData, int sampleRate) override;
};

// Real local STT backend powered by whisper.cpp.
class LocalWhisperSTT : public STTModel {
private:
    // whisper.cpp stores model state and inference context here after load.
    struct whisper_context* ctx;

public:
    explicit LocalWhisperSTT(const std::string& modelPath);
    ~LocalWhisperSTT() override;

    // Convert one captured utterance from raw PCM samples into text.
    std::string transcribe(const std::vector<int16_t>& audioData, int sampleRate) override;
};

#endif
