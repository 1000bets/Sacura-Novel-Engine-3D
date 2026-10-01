#pragma once
#include "Materials/MaterialRenderSnapshot.h"
#include "Materials/DynamicMaterialInstance.h"
#include "Reflection/ReflectionValueCodec.h"
#include <nlohmann/json.hpp>

struct MaterialSlotOverride
{
    int32_t Slot = 0;
    AssetKey Material;
};

struct MaterialSlotOverrides
{
    std::vector<MaterialSlotOverride> Entries;
    bool operator==(const MaterialSlotOverrides& Other) const;
};

struct MaterialPostProcessEffect
{
    AssetKey Material;
    bool bEnabled = true;
    float Intensity = 1.f;
    std::shared_ptr<DynamicMaterialInstance> DynamicMaterial;
    std::shared_ptr<const MaterialRenderSnapshot> Snapshot;
};

struct MaterialPostProcessEffects
{
    std::vector<MaterialPostProcessEffect> Entries;
    bool operator==(const MaterialPostProcessEffects& Other) const;
};

struct RenderPostProcessEffect
{
    std::shared_ptr<const MaterialRenderSnapshot> Snapshot;
    float Intensity = 1.f;
};

template <> struct ReflectionValueCodec<MaterialSlotOverrides>
{
    static TypeId GetTypeId() { return TypeId{"engine.MaterialSlots"}; }
    static ReflectionDiagnostic ToReflected(const MaterialSlotOverrides& Source, ReflectedValue& OutValue);
    static ReflectionDiagnostic FromReflected(MaterialSlotOverrides& Destination, const ReflectedValue& InValue);
};

template <> struct ReflectionValueCodec<MaterialPostProcessEffects>
{
    static TypeId GetTypeId() { return TypeId{"engine.PostProcessEffects"}; }
    static ReflectionDiagnostic ToReflected(const MaterialPostProcessEffects& Source, ReflectedValue& OutValue);
    static ReflectionDiagnostic FromReflected(MaterialPostProcessEffects& Destination, const ReflectedValue& InValue);
};
