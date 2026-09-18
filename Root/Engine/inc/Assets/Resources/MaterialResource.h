#pragma once

#include "Assets/AssetRef.h"
#include "Assets/Resources/TextureResource.h"

#include <SimpleMath.h>

struct MaterialResource
{
    DirectX::SimpleMath::Vector4 BaseColor = {1.f, 1.f, 1.f, 1.f};
    float Metallic = 0.f;
    float Roughness = 1.f;
    AssetRef<TextureResource> BaseColorTexture{};
};
