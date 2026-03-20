#include "AudioEngine.h"

#include <cmath>

// Add a freshly captured microphone chunk to the shared queue and wake any waiting consumer.
void AudioQueue::push(const std::vector<int16_t>& chunk) {
    std::lock_guard<std::mutex> lock(mtx);
    queue.push(chunk);
    cv.notify_one();
}

// Block until audio is available, then hand one chunk to the main loop.
std::vector<int16_t> AudioQueue::pop() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this]() { return !queue.empty(); });
    std::vector<int16_t> chunk = queue.front();
    queue.pop();
    return chunk;
}

// Drop any queued microphone audio.
// This is useful when the agent starts or stops talking so stale audio is not processed later.
void AudioQueue::clear() {
    std::lock_guard<std::mutex> lock(mtx);
    while (!queue.empty()) {
        queue.pop();
    }
}

// Naive VAD based on average absolute sample magnitude.
// The main loop uses this to decide when a spoken utterance starts and ends.
bool isVoiceActive(const std::vector<int16_t>& chunk) {
    double energy = 0.0;
    for (int16_t sample : chunk) {
        energy += std::abs(sample);
    }
    return (energy / chunk.size()) > 500.0;
}

// Convert one chunk into a single average loudness value.
// The callback smooths these values so sustained loud speech matters more than sharp spikes.
double getAverageChunkVolume(const std::vector<int16_t>& chunk) {
    double energy = 0.0;
    for (int16_t sample : chunk) {
        energy += std::abs(sample);
    }
    return energy / chunk.size();
}

int recordCallback(const void* inputBuffer, void* outputBuffer,
                   unsigned long framesPerBuffer,
                   const PaStreamCallbackTimeInfo* timeInfo,
                   PaStreamCallbackFlags statusFlags,
                   void* userData) {
    (void)outputBuffer;
    (void)timeInfo;
    (void)statusFlags;

    // PortAudio calls this very frequently, so it should stay lightweight.
    AudioData* data = (AudioData*)userData;
    const int16_t* in = (const int16_t*)inputBuffer;
    static double smoothedInterruptVolume = 0.0;
    const double smoothingFactor = 0.2;
    const double interruptAverageThreshold = 2500.0;

    if (in != nullptr) {
        std::vector<int16_t> chunk(in, in + framesPerBuffer);

        // While the agent is speaking, do not feed audio into STT.
        // Instead, watch for sustained louder microphone input that should interrupt TTS.
        if (data->agentSpeaking->load()) {
            double currentVolume = getAverageChunkVolume(chunk);
            smoothedInterruptVolume =
                (smoothingFactor * currentVolume) + ((1.0 - smoothingFactor) * smoothedInterruptVolume);

            // If the rolling average gets loud enough, ask the TTS layer to stop speaking.
            if (smoothedInterruptVolume >= interruptAverageThreshold) {
                data->interruptRequested->store(true);
                smoothedInterruptVolume = 0.0;
            }

            return paContinue;
        }

        // Once the agent is no longer speaking, reset interrupt-only state.
        smoothedInterruptVolume = 0.0;

        // Normal mode: forward microphone audio to the queue for the main loop / STT pipeline.
        if (data->sttEnabled->load()) {
            data->queue->push(chunk);
        }
    }

    return paContinue;
}
