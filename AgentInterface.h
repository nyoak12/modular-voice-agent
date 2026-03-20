#ifndef AGENT_INTERFACE_H
#define AGENT_INTERFACE_H

#include <string>

// Common interface for any chat/reasoning backend.
class AgentInterface {
public:
    virtual ~AgentInterface() = default;
    virtual std::string think(const std::string& userText) = 0;
};

#endif
