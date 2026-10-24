#pragma once
#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"
#include <SimpleMath.h>
using namespace DirectX::SimpleMath;

class SkyLightComponent : public Component
{
    ENGINE_CLASS(SkyLightComponent, Component, "engine.SkyLightComponent", "Sky Light")
public:
    std::string GetEditorIconPath() const override { return ":/component-icons/sun.png"; }
    Color LightColor = Color(1.f, 1.f, 1.f, 1.f);
    ENGINE_REFLECT_FIELD(SkyLightComponent, LightColor, "light_color", RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Light Color"))
    float Intensity = 1.f;
    ENGINE_REFLECT_FIELD(SkyLightComponent, Intensity, "intensity", RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Intensity"), RF_UI_RANGE(0.0, 100.0))
protected:
    SkyLightComponent() = default;
};
ENGINE_CLASS_END(SkyLightComponent)
ENGINE_IMPLEMENT_FIELD(SkyLightComponent, LightColor)
ENGINE_IMPLEMENT_FIELD(SkyLightComponent, Intensity)
