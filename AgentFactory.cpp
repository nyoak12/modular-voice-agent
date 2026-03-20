#include "AgentFactory.h"

#include "OllamaAgent.h"
#include "OpenAIResponsesAgent.h"

#include <iostream>

std::unique_ptr<AgentInterface> AgentFactory::create(const AgentConfig& config) {
    switch (config.provider) {
        case AgentProvider::Ollama:
            return std::make_unique<OllamaAgent>(config);
        case AgentProvider::OpenAIResponses:
            return std::make_unique<OpenAIResponsesAgent>(config);
        case AgentProvider::OpenAIResponsesMock:
            return std::make_unique<OpenAIResponsesAgent>(config);
    }

    std::cerr << "[AgentFactory] Unknown provider. Falling back to Ollama.\n";
    return std::make_unique<OllamaAgent>(config);
}
