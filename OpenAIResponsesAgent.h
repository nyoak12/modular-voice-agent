#ifndef OPENAI_RESPONSES_AGENT_H
#define OPENAI_RESPONSES_AGENT_H

#include "AgentConfig.h"
#include "AgentInterface.h"
#include "ConversationMemory.h"

#include <string>

// OpenAI Responses provider.
// This can call the real OpenAI API or stay in mock mode depending on AgentConfig.
class OpenAIResponsesAgent : public AgentInterface {
private:
    AgentConfig config;
    std::string systemPrompt;
    ConversationMemory memory;

    std::string loadSystemPrompt(const std::string& promptPath);
    std::string sanitize(std::string text);
    std::string extractOutputText(const std::string& jsonResponse);
    std::string extractErrorMessage(const std::string& jsonResponse);
    std::string buildRequestError(const std::string& details);

public:
    explicit OpenAIResponsesAgent(const AgentConfig& config);
    ~OpenAIResponsesAgent() override;

    std::string think(const std::string& userText) override;
};

#endif
