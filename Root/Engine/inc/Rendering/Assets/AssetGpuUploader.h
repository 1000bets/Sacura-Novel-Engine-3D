#pragma once

#include "Assets/AssetHandle.h"
#include "Assets/AssetTypes.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Rendering/RenderResourceHandles.h"
#include "Materials/MaterialCompilation.h"
#include "Materials/MaterialDefinition.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

enum class AssetGpuState
{
    Absent = 0,
    UploadQueued,
    Resident,
    Failed,
    Cancelled
};

struct AssetGpuMeshEntry
{
    AssetGpuState State = AssetGpuState::Absent;
    MeshHandle Mesh{};
    AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
    uint64_t SessionId = 0;
};

struct AssetGpuTextureEntry
{
    AssetGpuState State = AssetGpuState::Absent;
    TextureHandle Texture{};
    std::shared_ptr<const void> Lifetime;
    AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
    uint64_t SessionId = 0;
};

struct AssetGpuTextureKey
{
    AssetKey Asset;
    bool bSrgb = true;
    bool operator==(const AssetGpuTextureKey& Other) const
    {
        return Asset == Other.Asset && bSrgb == Other.bSrgb;
    }
};

struct AssetGpuTextureKeyHash
{
    size_t operator()(const AssetGpuTextureKey& Key) const
    {
        return AssetKeyHash{}(Key.Asset) ^ (static_cast<size_t>(Key.bSrgb) << 1);
    }
};

class AssetGpuUploader
{
public:
    void RequestUploadMesh(AssetHandle<StaticMeshResource> Mesh, const AssetKey& Key);
    void RequestUploadTexture(AssetHandle<TextureResource> Texture, const AssetKey& Key, bool bSrgb = true);
    std::shared_ptr<MaterialCompilation> RequestMaterial(const ResolvedMaterial& Material);

    AssetGpuState GetMeshState(const AssetKey& Key) const;
    AssetGpuState GetTextureState(const AssetKey& Key, bool bSrgb = true) const;
    bool TryGetMesh(const AssetKey& Key, MeshHandle& OutMesh) const;
    bool TryGetTexture(const AssetKey& Key, TextureHandle& OutTexture, bool bSrgb = true) const;
    std::shared_ptr<const void> GetTextureLifetime(const AssetKey& Key, bool bSrgb) const;

    void InvalidateAsset(const AssetId& Id);
    void Clear();
    uint64_t GetSessionId() const { return SessionId.load(std::memory_order_acquire); }

private:
    mutable std::mutex Mutex;
    std::unordered_map<AssetKey, AssetGpuMeshEntry, AssetKeyHash> Meshes;
    std::unordered_map<AssetGpuTextureKey, AssetGpuTextureEntry, AssetGpuTextureKeyHash> Textures;
    std::atomic<uint64_t> SessionId{1};
    std::unordered_map<std::string, std::shared_ptr<MaterialCompilation>> Materials;
};
