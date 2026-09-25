#pragma once

#include "Gameplay/Component.h"
#include "Reflection/ReflectionMacros.h"
#include "Rendering/RenderLight.h"

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class LightComponent : public Component
{
    ENGINE_CLASS(LightComponent, Component, "engine.LightComponent", 1)

public:
    RenderLightType Type = RenderLightType::Directional;
    Color LightColor = Color(1.f, 1.f, 1.f, 1.f);
    float Intensity = 1.f;
    float Range = 10.f;
    float InnerConeAngle = 0.f;
    float OuterConeAngle = 45.f;
    bool bCastShadows = false;

    int64_t GetLightTypeValue() const { return static_cast<int64_t>(Type); }
    void SetLightTypeValue(int64_t Value)
    {
        if (Value < static_cast<int64_t>(RenderLightType::Directional)
            || Value > static_cast<int64_t>(RenderLightType::Spot))
        {
            return;
        }
        Type = static_cast<RenderLightType>(Value);
    }

    ENGINE_REFLECT_PROPERTY(
        LightComponent,
        LightType,
        "light_type",
        int64_t,
        GetLightTypeValue,
        SetLightTypeValue,
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_UI_RANGE(0.0, 2.0),
        RF_DISPLAY_NAME("Light Type"))

    ENGINE_REFLECT_FIELD(
        LightComponent,
        LightColor,
        "light_color",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_DISPLAY_NAME("Light Color"))

    ENGINE_REFLECT_FIELD(
        LightComponent,
        Intensity,
        "intensity",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_UI_RANGE(0.0, 100.0),
        RF_DISPLAY_NAME("Intensity"))

    ENGINE_REFLECT_FIELD(
        LightComponent,
        Range,
        "range",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_UI_RANGE(0.0, 10000.0),
        RF_DISPLAY_NAME("Range"))

    ENGINE_REFLECT_FIELD(
        LightComponent,
        InnerConeAngle,
        "inner_cone_angle",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_DISPLAY_NAME("Inner Cone Angle"))

    ENGINE_REFLECT_FIELD(
        LightComponent,
        OuterConeAngle,
        "outer_cone_angle",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_DISPLAY_NAME("Outer Cone Angle"))

    ENGINE_REFLECT_FIELD(
        LightComponent,
        bCastShadows,
        "cast_shadows",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_SCRIPT_READ_WRITE,
        RF_DISPLAY_NAME("Cast Shadows"))

protected:
    LightComponent() = default;
};

ENGINE_CLASS_END(LightComponent)
ENGINE_IMPLEMENT_PROPERTY(LightComponent, LightType, int64_t, GetLightTypeValue, SetLightTypeValue)
ENGINE_IMPLEMENT_FIELD(LightComponent, LightColor)
ENGINE_IMPLEMENT_FIELD(LightComponent, Intensity)
ENGINE_IMPLEMENT_FIELD(LightComponent, Range)
ENGINE_IMPLEMENT_FIELD(LightComponent, InnerConeAngle)
ENGINE_IMPLEMENT_FIELD(LightComponent, OuterConeAngle)
ENGINE_IMPLEMENT_FIELD(LightComponent, bCastShadows)
