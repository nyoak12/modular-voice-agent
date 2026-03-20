#include "OpenAIResponsesAgent.h"

#include <algorithm>
#include <cstdlib>
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

std::string OpenAIResponsesAgent::loadSystemPrompt(const std::string& promptPath) {
    std::ifstream promptFile(promptPath);
    if (!promptFile.is_open()) {
        std::cerr << "[OpenAIResponsesAgent] Failed to open system prompt file: " << promptPath << "\n";
        return "Your name is 'Alex', an expert inbound sales agent for Apex Roofing.";
    }

    return std::string((std::istreambuf_iterator<char>(promptFile)), std::istreambuf_iterator<char>());
}

std::string OpenAIResponsesAgent::sanitize(std::string text) {
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

std::string OpenAIResponsesAgent::extractOutputText(const std::string& jsonResponse) {
    std::string typeMarker = "\"type\":\"output_text\"";
    size_t typePos = jsonResponse.find(typeMarker);

    if (typePos != std::string::npos) {
        std::string textMarker = "\"text\":\"";
        size_t textPos = jsonResponse.find(textMarker, typePos);
        if (textPos != std::string::npos) {
            textPos += textMarker.length();
            size_t endPos = jsonResponse.find("\"", textPos);
            std::string content = jsonResponse.substr(textPos, endPos - textPos);

            size_t pos = 0;
            while ((pos = content.find("\\n", pos)) != std::string::npos) {
                content.replace(pos, 2, "\n");
                pos += 1;
            }
            return content;
        }
    }

    std::string errorMessage = extractErrorMessage(jsonResponse);
    if (!errorMessage.empty()) {
        return "Error: OpenAI API returned an error. " + errorMessage;
    }

    return "Error: Could not parse OpenAI response. Raw: " + jsonResponse;
}

std::string OpenAIResponsesAgent::extractErrorMessage(const std::string& jsonResponse) {
    std::string marker = "\"message\":\"";
    size_t startPos = jsonResponse.find(marker);
    if (startPos == std::string::npos) {
        marker = "\"message\": \"";
        startPos = jsonResponse.find(marker);
    }
    if (startPos == std::string::npos) {
        return "";
    }

    startPos += marker.length();
    size_t endPos = jsonResponse.find("\"", startPos);
    return jsonResponse.substr(startPos, endPos - startPos);
}

std::string OpenAIResponsesAgent::buildRequestError(const std::string& details) {
    return "Error: OpenAI request failed. " + details;
}

OpenAIResponsesAgent::OpenAIResponsesAgent(const AgentConfig& configIn) : config(configIn) {
    systemPrompt = loadSystemPrompt(config.systemPromptPath);
    curl_global_init(CURL_GLOBAL_ALL);
    std::cout << "[Agent] OpenAI Responses provider initialized with model "
              << config.model << ".\n";
}

OpenAIResponsesAgent::~OpenAIResponsesAgent() {
    std::cout << "[OpenAIResponsesAgent] Destructor called. Releasing provider resources.\n";
    curl_global_cleanup();
}

std::string OpenAIResponsesAgent::think(const std::string& userText) {
    if (userText.empty()) return "";

    memory.addUserTurn(sanitize(userText));

    if (config.provider == AgentProvider::OpenAIResponsesMock) {
        std::string response =
            "[Mock OpenAI Responses] Provider switching works. "
            "Selected model: " + config.model +
            ". Last user message: " + userText;

        memory.addAssistantTurn(sanitize(response));
        return response;
    }

    const char* apiKey = std::getenv("OPENAI_API_KEY");
    if (apiKey == nullptr || std::string(apiKey).empty()) {
        return buildRequestError("OPENAI_API_KEY is not set in the environment.");
    }

    CURL* curl = curl_easy_init();
    std::string readBuffer;
    std::string requestError;

    if (!curl) {
        return buildRequestError("libcurl could not initialize.");
    }

    std::cout << "\n[Agent] Thinking with OpenAI Responses API...\n";

    curl_easy_setopt(curl, CURLOPT_URL, config.openaiUrl.c_str());

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string authHeader = "Authorization: Bearer " + std::string(apiKey);
    headers = curl_slist_append(headers, authHeader.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string jsonPayload = "{\n";
    jsonPayload += "  \"model\": \"" + sanitize(config.model) + "\",\n";
    jsonPayload += "  \"instructions\": \"" + sanitize(systemPrompt) + "\",\n";
    jsonPayload += "  \"input\": \"" + sanitize(memory.buildTaggedTranscript()) + "\",\n";
    jsonPayload += "  \"temperature\": " + std::to_string(config.temperature) + ",\n";
    jsonPayload += "  \"max_output_tokens\": " + std::to_string(config.maxOutputTokens);

    if (!config.reasoningEffort.empty()) {
        jsonPayload += ",\n  \"reasoning\": {\"effort\": \"" + sanitize(config.reasoningEffort) + "\"}";
    }

    jsonPayload += "\n}";

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
            return "Error: OpenAI HTTP " + std::to_string(httpStatus) + ". " + readBuffer;
        }
        return "Error: OpenAI HTTP " + std::to_string(httpStatus) + " with an empty response body.";
    }

    if (readBuffer.empty()) {
        return buildRequestError("the API returned an empty response body.");
    }

    std::string response = extractOutputText(readBuffer);
    if (response.find("Error:") == std::string::npos) {
        memory.addAssistantTurn(sanitize(response));
    }

    return response;
}
