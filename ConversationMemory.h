#ifndef CONVERSATION_MEMORY_H
#define CONVERSATION_MEMORY_H

#include <string>
#include <utility>
#include <vector>

// Shared conversation memory used by all agent providers.
// It stores the running dialog so the model can keep track of what has already
// been asked and answered across turns.
class ConversationMemory {
public:
    using Turn = std::pair<std::string, std::string>;

private:
    std::vector<Turn> turns;

public:
    void addUserTurn(const std::string& text);
    void addAssistantTurn(const std::string& text);
    const std::vector<Turn>& getTurns() const;
    std::string buildTaggedTranscript() const;
};

#endif
