#pragma once

#include "Authoring/AuthoringTypes.h"

#include <deque>

namespace Authoring
{
struct ExecutionLifetime
{
    bool bActive = true;
    ObjectHandle Owner;
};

struct SessionState
{
    struct Variable
    {
        ValueSchema Schema;
        ReflectedValue Value;
    };
    bool bRunning = false;
    double GameTime = 0.0;
    ObjectHandle World;
    std::map<std::string, Variable> Variables;
    std::map<std::pair<std::string, uint32_t>, SignalDefinition> SignalDefinitions;
    std::deque<Signal> Signals;
};

bool IsOwnerAvailable(const SessionState& State, ObjectHandle Owner);
}
