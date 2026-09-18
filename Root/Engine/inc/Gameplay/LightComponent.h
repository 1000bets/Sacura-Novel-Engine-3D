#pragma once

#include "Gameplay/Component.h"
#include "Rendering/RenderLight.h"

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

class LightComponent : public Component
{
    SAKURA_OBJECT(LightComponent)

public:
    RenderLightType Type = RenderLightType::Directional;
    Color LightColor = Color(1.f, 1.f, 1.f, 1.f);
    float Intensity = 1.f;
    float Range = 10.f;
    float InnerConeAngle = 0.f;
    float OuterConeAngle = 45.f;
    bool bCastShadows = false;

protected:
    LightComponent() = default;
};
