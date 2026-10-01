#pragma once


#include "Rendering/AxisAlignedBounds.h"
#include "Rendering/RenderResourceHandles.h"
#include "Materials/MaterialRenderSnapshot.h"

#include <SimpleMath.h>
#include <string>

using namespace DirectX::SimpleMath;

struct RenderObject
{
    Matrix WorldMatrix = Matrix::Identity;
    MeshHandle Mesh;
    std::shared_ptr<const MaterialRenderSnapshot> MaterialSnapshot;
    Color BaseColor = Color(1.f, 1.f, 1.f, 1.f);
    AxisAlignedBounds Bounds;
    uint32_t IndexOffset = 0;
    uint32_t IndexCount = 0;
    bool bVisible = true;
    bool bUsePlaceholder = false;
    bool bMissingAsset = false;
    std::string DiagnosticMessage;
};
