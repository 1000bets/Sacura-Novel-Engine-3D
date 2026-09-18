#pragma once

#include "Rendering/AxisAlignedBounds.h"

#include <SimpleMath.h>

#include <cstdint>
#include <vector>

struct StaticMeshVertex
{
    DirectX::SimpleMath::Vector3 Position{};
    DirectX::SimpleMath::Vector3 Normal{};
    DirectX::SimpleMath::Vector2 TexCoord{};
    bool bHasNormal = false;
    bool bHasTexCoord = false;
};

struct StaticMeshSubmesh
{
    uint32_t IndexOffset = 0;
    uint32_t IndexCount = 0;
    int32_t MaterialSlot = 0;
};

struct StaticMeshResource
{
    std::vector<StaticMeshVertex> Vertices;
    std::vector<uint32_t> Indices;
    std::vector<StaticMeshSubmesh> Submeshes;
    AxisAlignedBounds Bounds{};
};
