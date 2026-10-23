#pragma once
#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"
#include <SimpleMath.h>
using namespace DirectX::SimpleMath;

class SkySphereComponent : public Component
{
    ENGINE_CLASS(SkySphereComponent, Component, "engine.SkySphereComponent", "Небесная сфера")
public:
    std::string GetEditorIconPath() const override { return ":/component-icons/sun.png"; }
    Color ZenithColor = Color(0.35f, 0.55f, 1.1f, 1.f);
    ENGINE_REFLECT_FIELD(SkySphereComponent, ZenithColor, "zenith_color", RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Zenith Color"))
    Color HorizonColor = Color(0.45f, 0.55f, 0.7f, 1.f);
    ENGINE_REFLECT_FIELD(SkySphereComponent, HorizonColor, "horizon_color", RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Horizon Color"))
    Color GroundColor = Color(0.07f, 0.06f, 0.05f, 1.f);
    ENGINE_REFLECT_FIELD(SkySphereComponent, GroundColor, "ground_color", RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Ground Color"))
    Color SunColor = Color(1.f, 0.75f, 0.5f, 1.f);
    ENGINE_REFLECT_FIELD(SkySphereComponent, SunColor, "sun_color", RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Sun Color"))
    float SunIntensity = 4.f;
    ENGINE_REFLECT_FIELD(SkySphereComponent, SunIntensity, "sun_intensity", RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Sun Intensity"), RF_UI_RANGE(0.0, 100.0))
    float Brightness = 1.f;
    ENGINE_REFLECT_FIELD(SkySphereComponent, Brightness, "brightness", RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE, RF_DISPLAY_NAME("Brightness"), RF_UI_RANGE(0.0, 100.0))
protected:
    SkySphereComponent() = default;
};
ENGINE_CLASS_END(SkySphereComponent)
ENGINE_IMPLEMENT_FIELD(SkySphereComponent, ZenithColor)
ENGINE_IMPLEMENT_FIELD(SkySphereComponent, HorizonColor)
ENGINE_IMPLEMENT_FIELD(SkySphereComponent, GroundColor)
ENGINE_IMPLEMENT_FIELD(SkySphereComponent, SunColor)
ENGINE_IMPLEMENT_FIELD(SkySphereComponent, SunIntensity)
ENGINE_IMPLEMENT_FIELD(SkySphereComponent, Brightness)
