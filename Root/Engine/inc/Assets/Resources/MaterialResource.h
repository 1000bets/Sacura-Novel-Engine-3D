#pragma once

#include "Rendering/RenderMaterial.h"

#include "Assets/AssetRef.h"
#include "Assets/Resources/TextureResource.h"

#include <SimpleMath.h>

struct MaterialResource : RenderMaterial
{
    DirectX::SimpleMath::Vector4 BaseColor = {1.f, 1.f, 1.f, 1.f};
    AssetRef<TextureResource> BaseColorTexture{};
};
