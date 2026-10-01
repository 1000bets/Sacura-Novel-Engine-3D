#pragma once

#include "Rendering/Assets/AssetGpuUploader.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetRegistry.h"
#include "Materials/MaterialRenderSnapshot.h"
#include "Materials/DynamicMaterialInstance.h"
#include <chrono>
#include <filesystem>
#include "Core/Threading/JobSystem.h"

class Scene;

class SceneAssetResolver
{
public:
    void Bind(
        AssetRegistry* Registry,
        AssetManager* Manager,
        AssetGpuUploader* Uploader);

    void ResolveScene(Scene& TargetScene);
    std::shared_ptr<const MaterialRenderSnapshot> ResolveMaterial(const AssetKey& Key, const std::shared_ptr<DynamicMaterialInstance>& Dynamic = {});
    void PollMaterialChanges();
    void Clear();
    std::string GetMaterialDiagnostic(const AssetKey& Key) const;

private:
    AssetRegistry* BoundRegistry = nullptr;
    AssetManager* BoundManager = nullptr;
    AssetGpuUploader* BoundUploader = nullptr;
    struct FileRevision
    {
        std::filesystem::file_time_type Modified;
        uintmax_t Size = 0;
        bool bExists = false;
        bool operator==(const FileRevision& Other) const
        {
            return Modified == Other.Modified && Size == Other.Size && bExists == Other.bExists;
        }
    };
    struct TextureFingerprintPreparation
    {
        ContentHash Hash;
        AssetDiagnostic Diagnostic;
        FileRevision Revision;
    };
    struct WatchedTexture
    {
        AssetKey Asset;
        FileRevision Revision;
        Job PreparationJob;
        std::shared_ptr<TextureFingerprintPreparation> Prepared;
        std::chrono::steady_clock::time_point Changed;
        bool bPendingChange = false;
    };
    std::map<std::string, WatchedTexture> TextureFiles;
    struct CachedMaterial
    {
        std::shared_ptr<const MaterialRenderSnapshot> Published;
        std::map<std::string, FileRevision> Files;
        std::string Diagnostic;
        std::chrono::steady_clock::time_point Changed;
        bool bPendingChange = false;
    };
    struct CachedDynamicMaterial
    {
        std::weak_ptr<DynamicMaterialInstance> Instance;
        std::shared_ptr<const MaterialRenderSnapshot> Published;
        uint64_t Revision = 0;
    };
    std::unordered_map<const DynamicMaterialInstance*, CachedDynamicMaterial> DynamicMaterials;
    static FileRevision ReadFileRevision(const std::string& Path);
    std::unordered_map<AssetKey, CachedMaterial, AssetKeyHash> Materials;
    std::chrono::steady_clock::time_point LastPoll;
};
