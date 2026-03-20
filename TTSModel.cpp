#include "TTSModel.h"

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

// Escape single quotes before handing text to the macOS `say` command.
std::string MacOSTTS::escapeForShell(const std::string& text) {
    std::string escaped;
    escaped.reserve(text.size() + 16);

    for (char ch : text) {
        if (ch == '\'') {
            escaped += "'\\''";
        } else {
            escaped += ch;
        }
    }

    return escaped;
}

// Speak text by launching `say` as a subprocess.
// We keep polling while it runs so microphone activity can interrupt playback.
void MacOSTTS::speak(const std::string& text, std::atomic<bool>& interruptRequested) {
    if (text.empty()) return;

    std::cout << "[TTS] Speaking agent response...\n";

    std::string safeText = escapeForShell(text);
    pid_t pid = fork();

    // Child process: replace itself with the system speech command.
    if (pid == 0) {
        execlp("say", "say", safeText.c_str(), (char*)nullptr);
        _exit(1);
    }

    if (pid < 0) {
        std::cerr << "[TTS] Failed to start macOS 'say'.\n";
        return;
    }

    bool wasInterrupted = false;

    while (true) {
        int status = 0;
        pid_t waitResult = waitpid(pid, &status, WNOHANG);

        // The `say` process finished on its own.
        if (waitResult == pid) {
            if (!wasInterrupted && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) {
                std::cerr << "[TTS] macOS 'say' exited unexpectedly.\n";
            }
            break;
        }

        // The audio engine heard enough competing volume to treat it as a human interruption.
        if (interruptRequested.load()) {
            wasInterrupted = true;
            kill(pid, SIGTERM);
            waitpid(pid, nullptr, 0);
            std::cout << "[TTS] Speech interrupted by microphone activity.\n";
            break;
        }

        // Small sleep to avoid busy-waiting while we monitor the subprocess.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
