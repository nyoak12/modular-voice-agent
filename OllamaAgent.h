#ifndef OLLAMA_AGENT_H
#define OLLAMA_AGENT_H

#include "AgentConfig.h"
#include "AgentInterface.h"
#include "ConversationMemory.h"

#include <string>

class OllamaAgent : public AgentInterface {
private:
    AgentConfig config;
    std::string systemPrompt;
    ConversationMemory memory;

    std::string extractContent(const std::string& jsonResponse);
    std::string sanitize(std::string text);
    std::string loadSystemPrompt(const std::string& promptPath);
    std::string buildRequestError(const std::string& details);

public:
    explicit OllamaAgent(const AgentConfig& config);
    ~OllamaAgent() override;

    std::string think(const std::string& userText) override;
};

#endif
