#pragma once

#include "Reflection/ReflectionMacros.h"

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct LightSettings
{
    ENGINE_STRUCT(LightSettings, "engine.LightSettings", 1)

public:
    Color LightColor = Color(1.f, 1.f, 1.f, 1.f);
    float Intensity = 1.f;
    Vector3 Direction = Vector3(0.f, -1.f, 0.f);
    bool bCastShadows = true;

    ENGINE_REFLECT_FIELD(
        LightSettings,
        LightColor,
        "light_color",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_DISPLAY_NAME("Light Color"))

    ENGINE_REFLECT_FIELD(
        LightSettings,
        Intensity,
        "intensity",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_UI_RANGE(0.0, 100.0),
        RF_DISPLAY_NAME("Intensity"))

    ENGINE_REFLECT_FIELD(
        LightSettings,
        Direction,
        "direction",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_DISPLAY_NAME("Direction"))

    ENGINE_REFLECT_FIELD(
        LightSettings,
        bCastShadows,
        "cast_shadows",
        RF_EDITOR_EDITABLE,
        RF_SERIALIZABLE,
        RF_DISPLAY_NAME("Cast Shadows"))
};

ENGINE_STRUCT_END(LightSettings)
ENGINE_IMPLEMENT_FIELD(LightSettings, LightColor)
ENGINE_IMPLEMENT_FIELD(LightSettings, Intensity)
ENGINE_IMPLEMENT_FIELD(LightSettings, Direction)
ENGINE_IMPLEMENT_FIELD(LightSettings, bCastShadows)
