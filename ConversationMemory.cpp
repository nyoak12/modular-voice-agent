#include "ConversationMemory.h"

void ConversationMemory::addUserTurn(const std::string& text) {
    turns.push_back({"user", text});
}

void ConversationMemory::addAssistantTurn(const std::string& text) {
    turns.push_back({"assistant", text});
}

const std::vector<ConversationMemory::Turn>& ConversationMemory::getTurns() const {
    return turns;
}

std::string ConversationMemory::buildTaggedTranscript() const {
    std::string transcript;

    for (const auto& turn : turns) {
        if (turn.first == "user") {
            transcript += "User: " + turn.second + "\n";
        } else if (turn.first == "assistant") {
            transcript += "Assistant: " + turn.second + "\n";
        }
    }

    return transcript;
}
