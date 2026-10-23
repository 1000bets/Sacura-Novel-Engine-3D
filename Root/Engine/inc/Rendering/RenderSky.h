#pragma once
#include <SimpleMath.h>
using namespace DirectX::SimpleMath;

struct RenderSky
{
    bool bVisible = false;
    bool bLighting = false;
    bool bAuthored = false;
    Vector3 Zenith = Vector3(0.35f, 0.55f, 1.1f);
    Vector3 Horizon = Vector3(0.45f, 0.55f, 0.7f);
    Vector3 Ground = Vector3(0.07f, 0.06f, 0.05f);
    Vector3 SunColor = Vector3(4.f, 3.f, 2.f);
    Vector3 SunDirection = Vector3(0.57735f, 0.57735f, 0.57735f);
    Vector3 LightColor = Vector3::One;
    float Brightness = 1.f;
    float LightIntensity = 1.f;
};
