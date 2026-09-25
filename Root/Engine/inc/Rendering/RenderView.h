#pragma once

#include "Rendering/RenderCamera.h"

#include <SimpleMath.h>
#include <cstdint>

using namespace DirectX::SimpleMath;

struct RenderViewId
{
    uint32_t Value = 0;

    bool IsValid() const { return Value != 0; }
    bool operator==(const RenderViewId& Other) const { return Value == Other.Value; }
    bool operator!=(const RenderViewId& Other) const { return Value != Other.Value; }
};

struct RenderSurfaceId
{
    uint32_t Value = 0;

    bool IsValid() const { return Value != 0; }
    bool operator==(const RenderSurfaceId& Other) const { return Value == Other.Value; }
    bool operator!=(const RenderSurfaceId& Other) const { return Value != Other.Value; }
};

struct RenderViewCamera
{
    Vector3 Position = Vector3(0.f, 1.5f, -4.f);
    Vector3 Target = Vector3::Zero;
    Vector3 Up = Vector3::Up;
    float FieldOfViewDegrees = 60.f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.f;

    void BuildRenderCamera(float AspectRatio, RenderCamera& OutCamera) const;
};

struct RenderView
{
    RenderViewId Id{};
    RenderSurfaceId Surface{};
    RenderViewCamera Camera{};
    uint32_t Width = 1;
    uint32_t Height = 1;
    bool bEnabled = true;
};
