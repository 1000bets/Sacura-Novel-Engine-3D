#pragma once

#include "Gameplay/Component.h"
#include "Gameplay/ObjectHandle.h"
#include "Reflection/ReflectionMacros.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <unordered_map>

class ScriptComponent : public Component
{
    ENGINE_CLASS(ScriptComponent, Component, "engine.ScriptComponent", 1)

public:
    bool HasManagedProperty(const PropertyId& Id) const;
    bool TryGetManagedProperty(const PropertyId& Id, ReflectedValue& OutValue) const;
    void SetManagedProperty(const PropertyId& Id, ReflectedValue Value);
    void ClearManagedProperties();
    void CopyManagedPropertiesFrom(const ScriptComponent& Source);

    const std::unordered_map<PropertyId, ReflectedValue, PropertyIdHash>& GetManagedProperties() const
    {
        return ManagedProperties;
    }

    ScriptInstanceHandle GetScriptInstanceHandle() const { return ScriptInstance; }
    void SetScriptInstanceHandle(ScriptInstanceHandle Value) { ScriptInstance = Value; }

    bool IsScriptFailed() const { return bScriptFailed; }
    void MarkScriptFailed() { bScriptFailed = true; }

    bool HasCalledDestroy() const { return bDestroyCalled; }
    void MarkDestroyCalled() { bDestroyCalled = true; }

    void OnCreate() override;
    void OnDestroy() override;
    void Tick(float DeltaTime) override;

    static ReflectionDiagnostic ReadManagedProperty(
        Object* Instance,
        const PropertyDescriptor& Descriptor,
        ReflectedValue& OutValue);
    static ReflectionDiagnostic WriteManagedProperty(
        Object* Instance,
        const PropertyDescriptor& Descriptor,
        const ReflectedValue& InValue);

protected:
    ScriptComponent() = default;

private:
    std::unordered_map<PropertyId, ReflectedValue, PropertyIdHash> ManagedProperties;
    ScriptInstanceHandle ScriptInstance{};
    bool bScriptFailed = false;
    bool bDestroyCalled = false;
};

ENGINE_CLASS_END(ScriptComponent)
