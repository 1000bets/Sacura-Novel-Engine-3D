#include "Gameplay/ScriptComponent.h"

#include "Core/Threading/ThreadContext.h"
#include "Scripting/ScriptingSubsystem.h"

bool ScriptComponent::HasManagedProperty(const PropertyId& Id) const
{
    return ManagedProperties.find(Id) != ManagedProperties.end();
}

bool ScriptComponent::TryGetManagedProperty(const PropertyId& Id, ReflectedValue& OutValue) const
{
    auto Found = ManagedProperties.find(Id);
    if (Found == ManagedProperties.end())
    {
        return false;
    }
    OutValue = Found->second;
    return true;
}

void ScriptComponent::SetManagedProperty(const PropertyId& Id, ReflectedValue Value)
{
    ManagedProperties[Id] = std::move(Value);
}

void ScriptComponent::ClearManagedProperties()
{
    ManagedProperties.clear();
}

void ScriptComponent::CopyManagedPropertiesFrom(const ScriptComponent& Source)
{
    ManagedProperties = Source.ManagedProperties;
}

void ScriptComponent::OnCreate()
{
    AssertGameThread();
    ScriptingSubsystem::Get().BindScriptComponent(*this);
}

void ScriptComponent::OnDestroy()
{
    AssertGameThread();
    ScriptingSubsystem::Get().UnbindScriptComponent(*this);
}

void ScriptComponent::Tick(float DeltaTime)
{
    AssertGameThread();
    if (bScriptFailed)
    {
        return;
    }
    ScriptingSubsystem::Get().TickScriptComponent(*this, DeltaTime);
}

ReflectionDiagnostic ScriptComponent::ReadManagedProperty(
    Object* Instance,
    const PropertyDescriptor& Descriptor,
    ReflectedValue& OutValue)
{
    ScriptComponent* Script = dynamic_cast<ScriptComponent*>(Instance);
    if (Script == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is not ScriptComponent", Descriptor.DeclaringTypeId, Descriptor.Id);
    }
    if (!Script->TryGetManagedProperty(Descriptor.Id, OutValue))
    {
        return ReflectionDiagnostic::Fail("Managed property missing", Descriptor.DeclaringTypeId, Descriptor.Id);
    }
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ScriptComponent::WriteManagedProperty(
    Object* Instance,
    const PropertyDescriptor& Descriptor,
    const ReflectedValue& InValue)
{
    ScriptComponent* Script = dynamic_cast<ScriptComponent*>(Instance);
    if (Script == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is not ScriptComponent", Descriptor.DeclaringTypeId, Descriptor.Id);
    }
    Script->SetManagedProperty(Descriptor.Id, InValue);
    return ReflectionDiagnostic::Ok();
}
