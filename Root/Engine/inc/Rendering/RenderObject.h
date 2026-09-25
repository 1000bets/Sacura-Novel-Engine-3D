#pragma once

#include "Rendering/RenderMaterial.h"

#include "Rendering/AxisAlignedBounds.h"
#include "Rendering/RenderResourceHandles.h"

#include <SimpleMath.h>
#include <string>

using namespace DirectX::SimpleMath;

struct RenderObject
{
    Matrix WorldMatrix = Matrix::Identity;
    MeshHandle Mesh;
    MaterialHandle Material;
    TextureHandle BaseColorTexture;
    RenderMaterial SurfaceMaterial;
    Color BaseColor = Color(1.f, 1.f, 1.f, 1.f);
    AxisAlignedBounds Bounds;
    bool bVisible = true;
    bool bUsePlaceholder = false;
    bool bMissingAsset = false;
    std::string DiagnosticMessage;
};
