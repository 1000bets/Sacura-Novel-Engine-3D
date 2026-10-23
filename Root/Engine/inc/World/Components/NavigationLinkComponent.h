#pragma once

#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"
#include "Navigation/NavigationTypes.h"

class NavigationLinkComponent : public Component
{
    ENGINE_CLASS(NavigationLinkComponent, Component, "engine.NavigationLinkComponent", "Навигационная связь")
public:
    Vector3 Start = Vector3(-1.f, 0.f, 0.f);
    ENGINE_REFLECT_FIELD(NavigationLinkComponent, Start, "start", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Local Start"))
    Vector3 End = Vector3(1.f, 0.f, 0.f);
    ENGINE_REFLECT_FIELD(NavigationLinkComponent, End, "end", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Local End"))
    float Radius = 0.5f;
    ENGINE_REFLECT_FIELD(NavigationLinkComponent, Radius, "radius", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Connection Radius"), RF_UI_RANGE(0.001, 1000))
    bool bBidirectional = true;
    ENGINE_REFLECT_FIELD(NavigationLinkComponent, bBidirectional, "bidirectional", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Bidirectional"))
    int64_t Area = 1;
    ENGINE_REFLECT_FIELD(NavigationLinkComponent, Area, "area", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Area"), RF_UI_RANGE(1, 16))
    std::string AgentProfile = "";
    ENGINE_REFLECT_FIELD(NavigationLinkComponent, AgentProfile, "agent_profile", RF_EDITOR_EDITABLE, RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE, RF_CATEGORY("Navigation"), RF_DISPLAY_NAME("Agent Profile (empty for all)"))

protected:
    NavigationLinkComponent() = default;
};
ENGINE_CLASS_END(NavigationLinkComponent)
ENGINE_IMPLEMENT_FIELD(NavigationLinkComponent, Start)
ENGINE_IMPLEMENT_FIELD(NavigationLinkComponent, End)
ENGINE_IMPLEMENT_FIELD(NavigationLinkComponent, Radius)
ENGINE_IMPLEMENT_FIELD(NavigationLinkComponent, bBidirectional)
ENGINE_IMPLEMENT_FIELD(NavigationLinkComponent, Area)
ENGINE_IMPLEMENT_FIELD(NavigationLinkComponent, AgentProfile)

