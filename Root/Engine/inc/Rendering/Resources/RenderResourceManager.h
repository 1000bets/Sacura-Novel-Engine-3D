#pragma once

#include "Rendering/RenderResourceHandles.h"
#include "Rendering/Resources/RenderMesh.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Diligent
{
struct IRenderDevice;
struct ITexture;
struct ITextureView;
}

struct StaticMeshResource;
struct TextureResource;

class RenderResourceManager
{
public:
    void Initialize(Diligent::IRenderDevice* Device);
    void Shutdown();

    MeshHandle CreateDefaultQuadMesh();
    MeshHandle CreateMeshFromCpuData(const StaticMeshResource& Mesh);
    TextureHandle CreateTextureFromCpuData(const TextureResource& Texture);

    const RenderMesh* GetMesh(MeshHandle Mesh) const;
    Diligent::ITexture* GetTexture(TextureHandle Texture) const;
    Diligent::ITextureView* GetTextureShaderView(TextureHandle Texture) const;

private:
    struct GpuTextureEntry
    {
        Diligent::ITexture* Texture = nullptr;
        Diligent::ITextureView* ShaderView = nullptr;
    };

    Diligent::IRenderDevice* Device = nullptr;
    std::unordered_map<uint64_t, RenderMesh> Meshes;
    std::unordered_map<uint64_t, GpuTextureEntry> Textures;
    uint64_t NextMeshId = 1;
    uint64_t NextTextureId = 1;
};
