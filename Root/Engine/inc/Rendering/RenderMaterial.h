#pragma once

#include <SimpleMath.h>
#include <cstdint>

enum class MaterialAlphaMode : uint32_t
{
    Opaque,
    Mask,
    Blend
};

struct RenderMaterial
{
    float Metallic = 0.f;
    float Roughness = 1.f;
    float AlphaCutoff = 0.5f;
    MaterialAlphaMode AlphaMode = MaterialAlphaMode::Opaque;
    DirectX::SimpleMath::Vector3 Emissive = DirectX::SimpleMath::Vector3::Zero;
    bool bDoubleSided = false;
    bool bCastShadows = true;
};
