#include "AgentConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

namespace {
std::string trim(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        start++;
    }

    size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        end--;
    }

    return value.substr(start, end - start);
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}
}

AgentConfig loadAgentConfig(const std::string& configPath) {
    AgentConfig config;
    std::ifstream configFile(configPath);

    if (!configFile.is_open()) {
        std::cerr << "[AgentConfig] Could not open " << configPath
                  << ". Using built-in defaults.\n";
        return config;
    }

    std::string line;
    while (std::getline(configFile, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t equalsPos = line.find('=');
        if (equalsPos == std::string::npos) {
            continue;
        }

        std::string key = toLower(trim(line.substr(0, equalsPos)));
        std::string value = trim(line.substr(equalsPos + 1));

        if (key == "provider") {
            std::string lowerValue = toLower(value);
            if (lowerValue == "ollama") {
                config.provider = AgentProvider::Ollama;
            } else if (lowerValue == "openai_responses") {
                config.provider = AgentProvider::OpenAIResponses;
            } else if (lowerValue == "openai_responses_mock") {
                config.provider = AgentProvider::OpenAIResponsesMock;
            }
        } else if (key == "model") {
            config.model = value;
        } else if (key == "temperature") {
            config.temperature = std::stod(value);
        } else if (key == "max_output_tokens") {
            config.maxOutputTokens = std::stoi(value);
        } else if (key == "system_prompt_path") {
            config.systemPromptPath = value;
        } else if (key == "ollama_url") {
            config.ollamaUrl = value;
        } else if (key == "openai_url") {
            config.openaiUrl = value;
        } else if (key == "reasoning_effort") {
            config.reasoningEffort = value;
        }
    }

    return config;
}

std::string agentProviderToString(AgentProvider provider) {
    switch (provider) {
        case AgentProvider::Ollama:
            return "ollama";
        case AgentProvider::OpenAIResponses:
            return "openai_responses";
        case AgentProvider::OpenAIResponsesMock:
            return "openai_responses_mock";
    }

    return "unknown";
}
