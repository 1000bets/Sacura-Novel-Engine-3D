#pragma once

#include "Rendering/RenderResourceHandles.h"
#include "Rendering/RenderSettings.h"
#include "Rendering/RHI/RenderMesh.h"

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
    void ReleaseMesh(MeshHandle Mesh, uint64_t CompletionValue);
    void ReleaseTexture(TextureHandle Texture, uint64_t CompletionValue);
    void CollectRetired(uint64_t CompletedValue);
    void GetStatistics(RenderStatistics& Statistics) const;

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
    std::unordered_map<uint64_t, uint64_t> RetiredMeshes;
    std::unordered_map<uint64_t, uint64_t> RetiredTextures;
};
