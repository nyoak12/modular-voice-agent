#ifndef AGENT_FACTORY_H
#define AGENT_FACTORY_H

#include "AgentConfig.h"
#include "AgentInterface.h"

#include <memory>

class AgentFactory {
public:
    static std::unique_ptr<AgentInterface> create(const AgentConfig& config);
};

#endif
