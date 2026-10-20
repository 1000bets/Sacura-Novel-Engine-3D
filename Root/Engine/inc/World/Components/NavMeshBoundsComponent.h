#pragma once

#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"
#include "Navigation/NavigationTypes.h"

class NavMeshBoundsComponent : public Component
{
    ENGINE_CLASS(NavMeshBoundsComponent, Component, "engine.NavMeshBoundsComponent", 1)
public:
    Vector3 HalfExtents = Vector3(10.f, 3.f, 10.f);
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, HalfExtents, "half_extents", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Half Extents"))
    std::string AgentProfile = "Default";
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, AgentProfile, "agent_profile", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Agent Profile"))
    float AgentRadius = 0.35f;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, AgentRadius, "agent_radius", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Agent Radius"), RF_UI_RANGE(0.001, 10000))
    float AgentHeight = 1.9f;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, AgentHeight, "agent_height", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Agent Height"), RF_UI_RANGE(0.001, 10000))
    float MaximumStepHeight = 0.3f;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, MaximumStepHeight, "maximum_step_height", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Maximum Step Height"), RF_UI_RANGE(0, 1000))
    float MaximumSlopeDegrees = 45.f;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, MaximumSlopeDegrees, "maximum_slope_degrees", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Maximum Slope (degrees)"), RF_UI_RANGE(0, 89))
    float CellSize = 0.15f;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, CellSize, "cell_size", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Cell Size"), RF_UI_RANGE(0.001, 100))
    float CellHeight = 0.1f;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, CellHeight, "cell_height", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Cell Height"), RF_UI_RANGE(0.001, 100))
    int64_t TileSize = 64;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, TileSize, "tile_size", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Tile Size (cells)"), RF_UI_RANGE(16, 256))
    int64_t MinimumRegionArea = 8;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, MinimumRegionArea, "minimum_region_area", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Minimum Region Size (cells)"), RF_UI_RANGE(0, 1024))
    int64_t MergeRegionArea = 20;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, MergeRegionArea, "merge_region_area", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Merge Region Size (cells)"), RF_UI_RANGE(0, 1024))
    float MaximumSimplificationError = 1.3f;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, MaximumSimplificationError, "maximum_simplification_error", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Maximum Simplification Error"), RF_UI_RANGE(0, 10))
    int64_t GenerationMode = 0;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, GenerationMode, "generation_mode", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Generation (0 Static, 1 Dynamic, 2 Modifiers Only)"), RF_UI_RANGE(0, 2))
    bool bAutoBuild = true;
    ENGINE_REFLECT_FIELD(NavMeshBoundsComponent, bAutoBuild, "auto_build", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Build Automatically in Editor"))
    NavigationBuildSettings GetBuildSettings() const;
    AxisAlignedBounds GetWorldBounds() const;

protected:
    NavMeshBoundsComponent() = default;
};
ENGINE_CLASS_END(NavMeshBoundsComponent)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, HalfExtents)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, AgentProfile)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, AgentRadius)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, AgentHeight)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, MaximumStepHeight)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, MaximumSlopeDegrees)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, CellSize)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, CellHeight)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, TileSize)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, MinimumRegionArea)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, MergeRegionArea)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, MaximumSimplificationError)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, GenerationMode)
ENGINE_IMPLEMENT_FIELD(NavMeshBoundsComponent, bAutoBuild)

