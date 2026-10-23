#pragma once

#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"
#include "Navigation/NavigationTypes.h"

class NavigationModifierComponent : public Component
{
    ENGINE_CLASS(NavigationModifierComponent, Component, "engine.NavigationModifierComponent")
public:
    Vector3 HalfExtents = Vector3::One;
    ENGINE_REFLECT_FIELD(NavigationModifierComponent, HalfExtents, "half_extents", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Half Extents"))
    int64_t Area = 0;
    ENGINE_REFLECT_FIELD(NavigationModifierComponent, Area, "area", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Area (0 Blocked, 1 Default, 2-16 Custom)"), RF_UI_RANGE(0, 16))
    int64_t Priority = 0;
    ENGINE_REFLECT_FIELD(NavigationModifierComponent, Priority, "priority", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Priority"), RF_UI_RANGE(-10000, 10000))
    std::string AgentProfile = "";
    ENGINE_REFLECT_FIELD(NavigationModifierComponent, AgentProfile, "agent_profile", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Agent Profile (empty for all)"))

protected:
    NavigationModifierComponent() = default;
};
ENGINE_CLASS_END(NavigationModifierComponent)
ENGINE_IMPLEMENT_FIELD(NavigationModifierComponent, HalfExtents)
ENGINE_IMPLEMENT_FIELD(NavigationModifierComponent, Area)
ENGINE_IMPLEMENT_FIELD(NavigationModifierComponent, Priority)
ENGINE_IMPLEMENT_FIELD(NavigationModifierComponent, AgentProfile)

