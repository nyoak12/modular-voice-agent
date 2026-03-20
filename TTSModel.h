#ifndef TTS_MODEL_H
#define TTS_MODEL_H

#include <atomic>
#include <string>

// Common interface for text-to-speech backends.
class TTSModel {
public:
    virtual ~TTSModel() = default;
    virtual void speak(const std::string& text, std::atomic<bool>& interruptRequested) = 0;
};

// macOS-specific TTS backend that delegates speech output to the built-in `say` command.
class MacOSTTS : public TTSModel {
private:
    // Escape text so it can safely be passed to the speech subprocess.
    std::string escapeForShell(const std::string& text);

public:
    // Speak the agent reply and stop early if the audio engine requests an interruption.
    void speak(const std::string& text, std::atomic<bool>& interruptRequested) override;
};

#endif
