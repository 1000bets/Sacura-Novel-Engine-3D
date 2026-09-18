#pragma once

#include "Rendering/AxisAlignedBounds.h"
#include "Rendering/RenderResourceHandles.h"

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct RenderObject
{
    Matrix WorldMatrix = Matrix::Identity;
    MeshHandle Mesh;
    MaterialHandle Material;
    AxisAlignedBounds Bounds;
    bool bVisible = true;
};
