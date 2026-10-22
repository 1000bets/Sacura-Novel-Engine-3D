#include "Authoring/OperationContext.h"
#include "SessionState.h"

#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Reflection/PropertyAccess.h"
#include "World/Components/Component.h"
#include "World/GameObject.h"
#include "World/Scene.h"

#include <utility>

namespace Authoring
{
namespace
{
Object* ResolveTarget(const SessionState& State, ObjectHandle Identity)
{
    MemorySubsystem* Memory = MemorySubsystem::Get();
    Scene* World = Memory ? Memory->ResolveHandle<Scene>(State.World) : nullptr;
    Object* Target = World ? Memory->ResolveHandle(Identity) : nullptr;
    Component* ComponentInstance = dynamic_cast<Component*>(Target);
    GameObject* Owner = ComponentInstance ? ComponentInstance->GetGameObject() : dynamic_cast<GameObject*>(Target);
    if (!Owner || Owner->GetScene() != World || Owner->IsPendingDestroy()
        || (ComponentInstance && ComponentInstance->IsPendingRemoval()))
    {
        return nullptr;
    }
    return Target;
}
}

bool IsOwnerAvailable(const SessionState& State, ObjectHandle Owner)
{
    if (!State.World.IsValid())
    {
        return !Owner.IsValid();
    }
    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (!Memory || !Memory->ResolveHandle<Scene>(State.World))
    {
        return false;
    }
    if (!Owner.IsValid())
    {
        return true;
    }
    Object* Target = ResolveTarget(State, Owner);
    Component* ComponentInstance = dynamic_cast<Component*>(Target);
    GameObject* ObjectInstance = ComponentInstance ? ComponentInstance->GetGameObject() : dynamic_cast<GameObject*>(Target);
    return ObjectInstance && ObjectInstance->IsActiveInHierarchy()
        && (!ComponentInstance || ComponentInstance->IsEnabled());
}

OperationContext::OperationContext(std::weak_ptr<SessionState> InState, std::shared_ptr<ExecutionLifetime> InLifetime)
    : State(std::move(InState)), Lifetime(std::move(InLifetime))
{
}

bool OperationContext::IsActive() const
{
    AssertGameThread();
    const auto Current = State.lock();
    return Lifetime && Lifetime->bActive && Current && Current->bRunning && IsOwnerAvailable(*Current, Lifetime->Owner);
}

double OperationContext::GetGameTime() const
{
    return IsActive() ? State.lock()->GameTime : 0.0;
}

ObjectHandle OperationContext::GetOwner() const
{
    return IsActive() ? Lifetime->Owner : ObjectHandle{};
}

ObjectHandle OperationContext::GetWorld() const
{
    return IsActive() ? State.lock()->World : ObjectHandle{};
}

Diagnostic OperationContext::ReadVariable(const std::string& Id, ReflectedValue& OutValue) const
{
    if (!IsActive())
    {
        return Diagnostic::Failure("context", "Execution context is no longer active");
    }
    const auto Current = State.lock();
    const auto Found = Current->Variables.find(Id);
    if (Found == Current->Variables.end())
    {
        return Diagnostic::Failure("variables." + Id, "Variable is not declared");
    }
    OutValue = Found->second.Value;
    return {};
}

Diagnostic OperationContext::WriteVariable(const std::string& Id, const ReflectedValue& Value)
{
    if (!IsActive())
    {
        return Diagnostic::Failure("context", "Execution context is no longer active");
    }
    const auto Current = State.lock();
    const auto Found = Current->Variables.find(Id);
    if (Found == Current->Variables.end())
    {
        return Diagnostic::Failure("variables." + Id, "Variable is not declared");
    }
    const Diagnostic Valid = ValidateValue(Found->second.Schema, Value, "variables." + Id);
    if (Valid.bOk)
    {
        Found->second.Value = Value;
    }
    return Valid;
}

Diagnostic OperationContext::PublishSignal(const std::string& Id, uint32_t Version, const ReflectedValue& Payload)
{
    if (!IsActive())
    {
        return Diagnostic::Failure("context", "Execution context is no longer active");
    }
    const auto Current = State.lock();
    const auto Found = Current->SignalDefinitions.find({Id, Version});
    if (Found == Current->SignalDefinitions.end())
    {
        return Diagnostic::Failure("signal.id", "Signal or version is not declared");
    }
    const Diagnostic Valid = ValidateValue(Found->second.Payload, Payload, "signal.payload");
    if (!Valid.bOk)
    {
        return Valid;
    }
    if (Current->Signals.size() >= 128)
    {
        return Diagnostic::Failure("signals", "Pending signal limit reached");
    }
    Current->Signals.push_back({Id, Version, Payload, Lifetime->Owner, Current->GameTime});
    return {};
}

ObjectHandle OperationContext::FindObject(const std::string& PersistentId) const
{
    if (!IsActive())
    {
        return {};
    }
    MemorySubsystem* Memory = MemorySubsystem::Get();
    Scene* World = Memory ? Memory->ResolveHandle<Scene>(State.lock()->World) : nullptr;
    GameObject* Target = World ? World->FindByPersistentId(PersistentId) : nullptr;
    return Target && !Target->IsPendingDestroy() ? Target->GetObjectHandle() : ObjectHandle{};
}

Diagnostic OperationContext::ReadProperty(ObjectHandle Identity, const PropertyId& Property, ReflectedValue& OutValue) const
{
    if (!IsActive())
    {
        return Diagnostic::Failure("context", "Execution context is no longer active");
    }
    Object* Target = ResolveTarget(*State.lock(), Identity);
    if (!Target)
    {
        return Diagnostic::Failure("target", "Target is missing or belongs to another world");
    }
    const ReflectionDiagnostic Result = PropertyAccess::GetProperty(Target, Property, OutValue, PropertyAccessContext::Script);
    return Result.bOk ? Diagnostic{} : Diagnostic::Failure("target." + Property.Value, Result.Message);
}

Diagnostic OperationContext::WriteProperty(ObjectHandle Identity, const PropertyId& Property, const ReflectedValue& Value)
{
    if (!IsActive())
    {
        return Diagnostic::Failure("context", "Execution context is no longer active");
    }
    Object* Target = ResolveTarget(*State.lock(), Identity);
    if (!Target)
    {
        return Diagnostic::Failure("target", "Target is missing or belongs to another world");
    }
    const ReflectionDiagnostic Result = PropertyAccess::SetProperty(Target, Property, Value, PropertyAccessContext::Script);
    return Result.bOk ? Diagnostic{} : Diagnostic::Failure("target." + Property.Value, Result.Message);
}
}
