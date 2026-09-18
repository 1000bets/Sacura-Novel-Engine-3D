#pragma once

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct RenderCamera
{
    Matrix View = Matrix::Identity;
    Matrix Projection = Matrix::Identity;
    Matrix ViewProjection = Matrix::Identity;

    Vector3 Position = Vector3::Zero;

    float NearPlane = 0.1f;
    float FarPlane = 1000.f;
    float FieldOfView = 60.f;
    float AspectRatio = 16.f / 9.f;

    bool bValid = false;
};
