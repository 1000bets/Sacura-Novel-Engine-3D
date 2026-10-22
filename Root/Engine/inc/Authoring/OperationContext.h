#pragma once

#include "Authoring/AuthoringTypes.h"

#include <memory>

namespace Authoring
{
struct SessionState;
struct ExecutionLifetime;

// Copyable capability, revoked before cleanup and on every session restart.
class OperationContext
{
public:
    bool IsActive() const;
    double GetGameTime() const;
    ObjectHandle GetOwner() const;
    ObjectHandle GetWorld() const;
    Diagnostic ReadVariable(const std::string& Id, ReflectedValue& OutValue) const;
    Diagnostic WriteVariable(const std::string& Id, const ReflectedValue& Value);
    Diagnostic PublishSignal(const std::string& Id, uint32_t Version, const ReflectedValue& Payload);
    ObjectHandle FindObject(const std::string& PersistentId) const;
    Diagnostic ReadProperty(ObjectHandle Object, const PropertyId& Property, ReflectedValue& OutValue) const;
    Diagnostic WriteProperty(ObjectHandle Object, const PropertyId& Property, const ReflectedValue& Value);

private:
    friend class Runtime;
    OperationContext(std::weak_ptr<SessionState> State, std::shared_ptr<ExecutionLifetime> Lifetime);
    std::weak_ptr<SessionState> State;
    std::shared_ptr<ExecutionLifetime> Lifetime;
};
}
