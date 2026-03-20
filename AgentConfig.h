#ifndef AGENT_CONFIG_H
#define AGENT_CONFIG_H

#include <string>

enum class AgentProvider {
    Ollama,
    OpenAIResponses,
    OpenAIResponsesMock
};

// Small runtime configuration object for selecting the agent provider and model.
struct AgentConfig {
    AgentProvider provider = AgentProvider::Ollama;
    std::string model = "qwen2.5:14b";
    double temperature = 0.5;
    int maxOutputTokens = 125;
    std::string systemPromptPath = "system_prompt.txt";
    std::string ollamaUrl = "http://localhost:11434/v1/chat/completions";
    std::string openaiUrl = "https://api.openai.com/v1/responses";
    std::string reasoningEffort = "medium";
};

AgentConfig loadAgentConfig(const std::string& configPath = "agent_config.txt");
std::string agentProviderToString(AgentProvider provider);

#endif
