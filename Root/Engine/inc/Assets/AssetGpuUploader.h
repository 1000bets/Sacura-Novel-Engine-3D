#pragma once

#include "Assets/AssetHandle.h"
#include "Assets/AssetTypes.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Rendering/RenderResourceHandles.h"

#include <mutex>
#include <unordered_map>

enum class AssetGpuState
{
    Absent = 0,
    UploadQueued,
    Resident,
    Failed
};

struct AssetGpuMeshEntry
{
    AssetGpuState State = AssetGpuState::Absent;
    MeshHandle Mesh{};
    AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
};

struct AssetGpuTextureEntry
{
    AssetGpuState State = AssetGpuState::Absent;
    TextureHandle Texture{};
    AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
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

    void Clear();

private:
    mutable std::mutex Mutex;
    std::unordered_map<AssetKey, AssetGpuMeshEntry, AssetKeyHash> Meshes;
    std::unordered_map<AssetKey, AssetGpuTextureEntry, AssetKeyHash> Textures;
};
