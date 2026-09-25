#pragma once

#include "Assets/AssetHandle.h"
#include "Assets/AssetTypes.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Rendering/RenderResourceHandles.h"

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
    AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
    uint64_t SessionId = 0;
};

class AssetGpuUploader
{
public:
    void RequestUploadMesh(AssetHandle<StaticMeshResource> Mesh, const AssetKey& Key);
    void RequestUploadTexture(AssetHandle<TextureResource> Texture, const AssetKey& Key);

    AssetGpuState GetMeshState(const AssetKey& Key) const;
    AssetGpuState GetTextureState(const AssetKey& Key) const;
    bool TryGetMesh(const AssetKey& Key, MeshHandle& OutMesh) const;
    bool TryGetTexture(const AssetKey& Key, TextureHandle& OutTexture) const;

    void InvalidateAsset(const AssetId& Id);
    void Clear();
    uint64_t GetSessionId() const { return SessionId.load(std::memory_order_acquire); }

private:
    mutable std::mutex Mutex;
    std::unordered_map<AssetKey, AssetGpuMeshEntry, AssetKeyHash> Meshes;
    std::unordered_map<AssetKey, AssetGpuTextureEntry, AssetKeyHash> Textures;
    std::atomic<uint64_t> SessionId{1};
};
