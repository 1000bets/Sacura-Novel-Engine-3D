#pragma once

#include "Gameplay/Component.h"

class CameraComponent : public Component
{
    SAKURA_OBJECT(CameraComponent)

public:
    float FieldOfViewDegrees = 60.f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.f;
    float AspectRatio = 16.f / 9.f;
    bool bPrimary = true;

protected:
    CameraComponent() = default;
};
