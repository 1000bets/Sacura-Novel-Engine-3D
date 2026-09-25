#pragma once

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct RenderVertex
{
    Vector3 Position = Vector3::Zero;
    Vector3 Normal = Vector3::Up;
    Vector2 TexCoord = Vector2::Zero;
    DirectX::SimpleMath::Color VertexColor = DirectX::SimpleMath::Color(1.f, 1.f, 1.f, 1.f);
};

struct FrameConstants
{
    Matrix ViewProjection = Matrix::Identity;
};

struct ObjectConstants
{
    Matrix World = Matrix::Identity;
    Color BaseColor = Color(1.f, 1.f, 1.f, 1.f);
    float UseTexture = 0.f;
    float Padding0 = 0.f;
    float Padding1 = 0.f;
    float Padding2 = 0.f;
};
