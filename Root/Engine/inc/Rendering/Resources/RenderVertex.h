#pragma once

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct RenderVertex
{
    Vector3 Position;
    Color Color;
};

struct FrameConstants
{
    Matrix ViewProjection;
};

struct ObjectConstants
{
    Matrix World;
};
