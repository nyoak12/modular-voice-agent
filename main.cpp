#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <portaudio.h>
#include "AudioEngine.h"
#include "AgentConfig.h"
#include "AgentFactory.h"
#include "STTModel.h"
#include "TTSModel.h"
#include <atomic>
#include <csignal>

#define SAMPLE_RATE 16000
#define CHUNK_SIZE_MS 20 //keep 20-30 ms
#define FRAMES_PER_BUFFER (SAMPLE_RATE * CHUNK_SIZE_MS / 1000)

// Shared audio capture settings.
// 16 kHz is the expected input rate for the Whisper model used below.

namespace {
std::atomic<bool>* gIsRunning = nullptr;
std::atomic<bool>* gInterruptRequested = nullptr;

void handleSignal(int signal) {
    if (signal == SIGINT) {
        if (gIsRunning != nullptr) {
            gIsRunning->store(false);
        }

        if (gInterruptRequested != nullptr) {
            gInterruptRequested->store(true);
        }
    }
}
}

// --- 5. THE MAIN LOOP (Consumer & Endpoint Detection) ---
// Initializes audio capture, waits for speech, transcribes completed utterances,
// and forwards the text to the local LLM agent.
int main() {
    Pa_Initialize();
    
    AudioQueue sharedQueue;
    std::atomic<bool> isRunning(true);
    std::atomic<bool> sttEnabled(true);
    std::atomic<bool> agentSpeaking(false);
    std::atomic<bool> interruptRequested(false);
    AudioData sessionData = { &sharedQueue, &sttEnabled, &agentSpeaking, &interruptRequested };

    gIsRunning = &isRunning;
    gInterruptRequested = &interruptRequested;
    std::signal(SIGINT, handleSignal);
    
    // Open the default input device as a mono 16-bit microphone stream.
    PaStream *stream;
    Pa_OpenDefaultStream(&stream, 1, 0, paInt16, SAMPLE_RATE, FRAMES_PER_BUFFER, recordCallback, &sessionData);
    Pa_StartStream(stream);

    AgentConfig agentConfig = loadAgentConfig();
    std::cout << "[Main] Agent provider: " << agentProviderToString(agentConfig.provider)
              << ", model: " << agentConfig.model << "\n";

    // Instantiate Layer 2: STT (Local Whisper).
    STTModel* sttEngine = new LocalWhisperSTT("ggml-base.en.bin");

    // Instantiate Layer 3: Agent provider selected by config.
    std::unique_ptr<AgentInterface> agent = AgentFactory::create(agentConfig);

    // Instantiate Layer 4: TTS (macOS built-in speech synthesis).
    TTSModel* ttsEngine = new MacOSTTS();

    std::cout << "Microphone LIVE. Speak, then pause for half a second to trigger transcription and AI reasoning...\n";

    // This buffer accumulates one full spoken utterance until a pause is detected.
    std::vector<int16_t> utteranceBuffer;
    int silenceThreshold = 40; // thresh * chunk size = total silence time -> 600-800ms for natural feel
    int silentChunks = 0;
    bool isRecordingUtterance = false;

    while (isRunning.load()) {
        // Block until the audio thread has produced another chunk.
        std::vector<int16_t> chunk = sharedQueue.pop(); 

        if (!isRunning.load()) {
            break;
        }
        
        if (isVoiceActive(chunk)) {
            // We are inside speech, so keep extending the active utterance.
            isRecordingUtterance = true;
            silentChunks = 0; // Reset silence counter.
            utteranceBuffer.insert(utteranceBuffer.end(), chunk.begin(), chunk.end());
            std::cout << "\r[Main] Capturing voice...      " << std::flush;
        } else {
            if (isRecordingUtterance) {
                // Once speech has started, keep a short trailing silence so the utterance ends naturally.
                silentChunks++;
                utteranceBuffer.insert(utteranceBuffer.end(), chunk.begin(), chunk.end()); 
            }
        }

        // When enough silence follows speech, treat that as the end of the user's turn.
        if (isRecordingUtterance && silentChunks > silenceThreshold) {
            std::cout << "\n[Main] Endpoint detected! Sending to STT...\n";
            
            // Layer 2: convert captured audio into text.
            std::string userText = sttEngine->transcribe(utteranceBuffer, SAMPLE_RATE);
            std::cout << "\n[USER]: " << userText << "\n";
            
            // Layer 3: send meaningful transcriptions to the LLM.
            // The length check helps filter out empty transcripts and tiny bursts of noise.
            if (!userText.empty()) { 
                std::string aiResponse = agent->think(userText);
                std::cout << "\n[AGENT]: " << aiResponse << "\n\n";

                // Temporarily disable microphone-to-STT flow so the agent does not hear itself.
                sttEnabled.store(false);
                agentSpeaking.store(true);
                interruptRequested.store(false);
                sharedQueue.clear();
                utteranceBuffer.clear();
                silentChunks = 0;
                isRecordingUtterance = false;

                ttsEngine->speak(aiResponse, interruptRequested);

                // Re-enable STT as soon as the agent finishes speaking.
                agentSpeaking.store(false);
                sttEnabled.store(true);
                interruptRequested.store(false);
                sharedQueue.clear();
            }
            
            // Reset state so the next spoken utterance starts with a clean buffer.
            utteranceBuffer.clear();
            silentChunks = 0;
            isRecordingUtterance = false;
            std::cout << "Listening...\n";
        }
    }

    std::cout << "\n[Main] Shutdown requested. Cleaning up resources...\n";

    delete ttsEngine;
    delete sttEngine;
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    gIsRunning = nullptr;
    gInterruptRequested = nullptr;
    std::cout << "[Main] Shutdown complete.\n";
    return 0;
}
