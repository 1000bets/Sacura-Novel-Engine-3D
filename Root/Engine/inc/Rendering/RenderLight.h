#pragma once

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

enum class RenderLightType
{
    Directional,
    Point,
    Spot
};

struct RenderLight
{
    RenderLightType Type = RenderLightType::Directional;

    Vector3 Position = Vector3::Zero;
    Vector3 Direction = Vector3::Forward;

    Color LightColor = Color(1.f, 1.f, 1.f, 1.f);
    float Intensity = 1.f;
    float Range = 10.f;

    float InnerConeAngle = 0.f;
    float OuterConeAngle = 45.f;

    bool bCastShadows = true;
};
