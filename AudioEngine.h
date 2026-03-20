#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <vector>

#include <portaudio.h>

// Thread-safe inbox for microphone chunks.
// PortAudio pushes audio here from its callback thread, and the main loop pops from it.
class AudioQueue {
private:
    std::queue<std::vector<int16_t>> queue;
    std::mutex mtx;
    std::condition_variable cv;

public:
    void push(const std::vector<int16_t>& chunk);
    std::vector<int16_t> pop();
    void clear();
};

// Shared state passed into the PortAudio callback.
// These flags let the callback know whether it should queue normal STT audio
// or switch into the special "interrupt the agent while speaking" mode.
struct AudioData {
    AudioQueue* queue;
    std::atomic<bool>* sttEnabled;
    std::atomic<bool>* agentSpeaking;
    std::atomic<bool>* interruptRequested;
};

// Basic VAD helper used by the main loop to decide whether a chunk looks like speech.
bool isVoiceActive(const std::vector<int16_t>& chunk);

// Helper used by the callback while the agent is speaking.
// This gives us a single loudness number that can be smoothed over time.
double getAverageChunkVolume(const std::vector<int16_t>& chunk);

// PortAudio microphone callback.
// This is the "producer" side of the audio pipeline: it receives live mic audio
// and either queues it for STT or watches for an interruption while TTS is active.
int recordCallback(const void* inputBuffer, void* outputBuffer,
                   unsigned long framesPerBuffer,
                   const PaStreamCallbackTimeInfo* timeInfo,
                   PaStreamCallbackFlags statusFlags,
                   void* userData);

#endif
