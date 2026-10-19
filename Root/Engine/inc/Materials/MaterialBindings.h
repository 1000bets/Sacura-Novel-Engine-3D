#pragma once
#include "Materials/MaterialRenderSnapshot.h"
#include "Materials/DynamicMaterialInstance.h"
#include "Reflection/ReflectionMacros.h"
#include <nlohmann/json.hpp>

struct MaterialSlotOverride
{
    ENGINE_STRUCT(MaterialSlotOverride, "engine.MaterialSlotOverride", 1)
public:
    int32_t Slot = 0;
    AssetKey Material;
    ENGINE_REFLECT_FIELD(MaterialSlotOverride, Slot, "slot", RF_SERIALIZABLE, RF_EDITOR_EDITABLE, RF_COLLECTION_INDEX, RF_UI_RANGE(0.0, 2147483647.0))
    ENGINE_REFLECT_FIELD(MaterialSlotOverride, Material, "material", RF_SERIALIZABLE, RF_EDITOR_EDITABLE, RF_ASSET_TYPES("Material", "MaterialInstance"))
};

struct MaterialSlotOverrides
{
    ENGINE_STRUCT(MaterialSlotOverrides, "engine.MaterialSlots", 1)
public:
    std::vector<MaterialSlotOverride> Entries;
    bool operator==(const MaterialSlotOverrides& Other) const;
    ENGINE_REFLECT_FIELD(MaterialSlotOverrides, Entries, "entries", RF_SERIALIZABLE, RF_EDITOR_EDITABLE)
};

struct MaterialPostProcessEffect
{
    ENGINE_STRUCT(MaterialPostProcessEffect, "engine.MaterialPostProcessEffect", 1)
public:
    AssetKey Material;
    bool bEnabled = true;
    float Intensity = 1.f;
    std::shared_ptr<DynamicMaterialInstance> DynamicMaterial;
    std::shared_ptr<const MaterialRenderSnapshot> Snapshot;
    ENGINE_REFLECT_FIELD(MaterialPostProcessEffect, Material, "material", RF_SERIALIZABLE, RF_EDITOR_EDITABLE, RF_ASSET_TYPES("Material", "MaterialInstance"))
    ENGINE_REFLECT_FIELD(MaterialPostProcessEffect, bEnabled, "enabled", RF_SERIALIZABLE, RF_EDITOR_EDITABLE)
    ENGINE_REFLECT_FIELD(MaterialPostProcessEffect, Intensity, "intensity", RF_SERIALIZABLE, RF_EDITOR_EDITABLE, RF_UI_RANGE(0.0, 1.0))
};

struct MaterialPostProcessEffects
{
    ENGINE_STRUCT(MaterialPostProcessEffects, "engine.PostProcessEffects", 1)
public:
    std::vector<MaterialPostProcessEffect> Entries;
    bool operator==(const MaterialPostProcessEffects& Other) const;
    ENGINE_REFLECT_FIELD(MaterialPostProcessEffects, Entries, "entries", RF_SERIALIZABLE, RF_EDITOR_EDITABLE)
};

struct RenderPostProcessEffect
{
    std::shared_ptr<const MaterialRenderSnapshot> Snapshot;
    float Intensity = 1.f;
};

ENGINE_STRUCT_END(MaterialSlotOverride)
ENGINE_IMPLEMENT_FIELD(MaterialSlotOverride, Slot)
ENGINE_IMPLEMENT_FIELD(MaterialSlotOverride, Material)
ENGINE_STRUCT_END(MaterialSlotOverrides)
ENGINE_IMPLEMENT_FIELD(MaterialSlotOverrides, Entries)
ENGINE_STRUCT_END(MaterialPostProcessEffect)
ENGINE_IMPLEMENT_FIELD(MaterialPostProcessEffect, Material)
ENGINE_IMPLEMENT_FIELD(MaterialPostProcessEffect, bEnabled)
ENGINE_IMPLEMENT_FIELD(MaterialPostProcessEffect, Intensity)
ENGINE_STRUCT_END(MaterialPostProcessEffects)
ENGINE_IMPLEMENT_FIELD(MaterialPostProcessEffects, Entries)
