#pragma once
#include "Materials/MaterialDefinition.h"
#include "Rendering/RenderResourceHandles.h"
#include <memory>

struct MaterialTextureBinding
{
    std::string Identifier;
    TextureHandle Texture;
    std::shared_ptr<const void> Lifetime;
};

struct MaterialRenderSnapshot
{
    std::shared_ptr<const ResolvedMaterial> Material;
    std::vector<DirectX::SimpleMath::Vector4> Parameters;
    std::vector<MaterialTextureBinding> Textures;
    uint64_t Revision = 0;
};
