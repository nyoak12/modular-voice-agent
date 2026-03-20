#include "OllamaAgent.h"

#include <algorithm>
#include <curl/curl.h>
#include <fstream>
#include <iostream>

namespace {
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    ((std::string*)userp)->append((char*)contents, totalSize);
    return totalSize;
}
}

std::string OllamaAgent::extractContent(const std::string& jsonResponse) {
    std::string target = "\"content\":\"";
    size_t startPos = jsonResponse.find(target);
    if (startPos == std::string::npos) {
        target = "\"content\": \"";
        startPos = jsonResponse.find(target);
    }
    if (startPos == std::string::npos) {
        return "Error: Could not parse LLM response. Raw: " + jsonResponse;
    }

    startPos += target.length();
    size_t endPos = jsonResponse.find("\"", startPos);
    std::string content = jsonResponse.substr(startPos, endPos - startPos);

    size_t pos = 0;
    while ((pos = content.find("\\n", pos)) != std::string::npos) {
        content.replace(pos, 2, "\n");
        pos += 1;
    }
    return content;
}

std::string OllamaAgent::sanitize(std::string text) {
    std::replace(text.begin(), text.end(), '"', '\'');

    size_t pos = 0;
    while ((pos = text.find('\n', pos)) != std::string::npos) {
        text.replace(pos, 1, " ");
        pos += 1;
    }

    pos = 0;
    while ((pos = text.find('\r', pos)) != std::string::npos) {
        text.replace(pos, 1, " ");
        pos += 1;
    }

    pos = 0;
    while ((pos = text.find('\\', pos)) != std::string::npos) {
        text.replace(pos, 1, "\\\\");
        pos += 2;
    }

    return text;
}

std::string OllamaAgent::loadSystemPrompt(const std::string& promptPath) {
    std::ifstream promptFile(promptPath);
    if (!promptFile.is_open()) {
        std::cerr << "[OllamaAgent] Failed to open system prompt file: " << promptPath << "\n";
        return "Your name is 'Alex', an expert inbound sales agent for Apex Roofing. Keep your answers brief, under 2 sentences. Your goal is to qualify the lead. Ask how old their roof is. Then ask if they have active leaks. Then ask if they filed an insurance claim. Only ask one question at a time.";
    }

    return std::string((std::istreambuf_iterator<char>(promptFile)), std::istreambuf_iterator<char>());
}

std::string OllamaAgent::buildRequestError(const std::string& details) {
    return "Error: Ollama request failed. " + details;
}

OllamaAgent::OllamaAgent(const AgentConfig& configIn) : config(configIn) {
    systemPrompt = loadSystemPrompt(config.systemPromptPath);
    curl_global_init(CURL_GLOBAL_ALL);
    std::cout << "[Agent] Ollama provider initialized with model " << config.model << ".\n";
}

OllamaAgent::~OllamaAgent() {
    std::cout << "[OllamaAgent] Destructor called. Releasing curl resources and conversation memory.\n";
    curl_global_cleanup();
}

std::string OllamaAgent::think(const std::string& userText) {
    if (userText.empty()) return "";

    memory.addUserTurn(sanitize(userText));

    CURL* curl = curl_easy_init();
    std::string readBuffer;
    std::string requestError;

    if (!curl) {
        return buildRequestError("libcurl could not initialize.");
    }

    std::cout << "\n[Agent] Thinking locally with Ollama...\n";

    curl_easy_setopt(curl, CURLOPT_URL, config.ollamaUrl.c_str());
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string jsonPayload = "{\n";
    jsonPayload += "  \"model\": \"" + sanitize(config.model) + "\",\n";
    jsonPayload += "  \"temperature\": " + std::to_string(config.temperature) + ",\n";
    jsonPayload += "  \"max_tokens\": " + std::to_string(config.maxOutputTokens) + ",\n";
    jsonPayload += "  \"messages\": [\n";
    jsonPayload += "    {\"role\": \"system\", \"content\": \"" + sanitize(systemPrompt) + "\"},\n";

    const auto& turns = memory.getTurns();
    for (size_t i = 0; i < turns.size(); ++i) {
        jsonPayload += "    {\"role\": \"" + turns[i].first + "\", \"content\": \"" + turns[i].second + "\"}";

        if (i < turns.size() - 1) {
            jsonPayload += ",\n";
        } else {
            jsonPayload += "\n";
        }
    }
    jsonPayload += "  ]\n}";

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        requestError = buildRequestError(curl_easy_strerror(res));
        std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
    }

    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (!requestError.empty()) {
        return requestError;
    }

    if (httpStatus >= 400) {
        if (!readBuffer.empty()) {
            return "Error: Ollama HTTP " + std::to_string(httpStatus) + ". " + readBuffer;
        }
        return "Error: Ollama HTTP " + std::to_string(httpStatus) + " with an empty response body.";
    }

    if (readBuffer.empty()) {
        return buildRequestError("Ollama returned an empty response body. Check that `ollama serve` is running and the selected model is installed.");
    }

    std::string aiResponse = extractContent(readBuffer);

    if (aiResponse.find("Error:") == std::string::npos) {
        memory.addAssistantTurn(sanitize(aiResponse));
    }

    return aiResponse;
}
