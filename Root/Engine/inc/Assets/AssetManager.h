#pragma once

#include "Assets/AssetHandle.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetTypes.h"
#include "Assets/Loaders/IAssetLoader.h"
#include "Assets/Loaders/MaterialLoader.h"
#include "Assets/Loaders/ModelLoader.h"
#include "Assets/Loaders/SkeletalLoader.h"
#include "Assets/Loaders/TextureLoader.h"
#include "Assets/Resources/AnimationClipResource.h"
#include "Assets/Resources/MaterialResource.h"
#include "Assets/Resources/ModelResource.h"
#include "Assets/Resources/SkeletalMeshResource.h"
#include "Assets/Resources/SkeletonResource.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Core/Threading/JobSystem.h"

#include <cstdint>
#include <mutex>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <vector>

enum class AssetLoadState
{
    Unloaded = 0,
    Queued,
    Loading,
    Ready,
    Failed,
    Cancelled
};

class AssetManager
{
public:
    AssetManager();
    void Initialize(AssetRegistry& Registry, JobSystem& Jobs);
    void Shutdown();

    template <typename ResourceType>
    void LoadAsync(const AssetKey& Key);

    template <typename ResourceType>
    bool TryGetLoaded(const AssetKey& Key, AssetHandle<ResourceType>& OutHandle) const;

    AssetLoadState GetLoadState(const AssetKey& Key) const;
    AssetDiagnostic GetLastDiagnostic(const AssetKey& Key) const;

    void PumpCompletions();
    void UnloadUnused();

private:
    struct SlotKey
    {
        AssetKey Key{};
        uint64_t Generation = 0;

        bool operator==(const SlotKey& Other) const
        {
            return Key == Other.Key && Generation == Other.Generation;
        }
    };

    struct SlotKeyHash
    {
        size_t operator()(const SlotKey& Value) const
        {
            AssetKeyHash HashKey;
            return HashKey(Value.Key) ^ (static_cast<size_t>(Value.Generation) * 0x9e3779b97f4a7c15ull);
        }
    };

    struct AssetSlot
    {
        AssetKey Key{};
        uint64_t Generation = 0;
        AssetLoadState State = AssetLoadState::Unloaded;
        AssetType Type = AssetType::Unknown;
        std::shared_ptr<const void> Resource;
        AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
        std::vector<AssetKey> PendingDependencies;
        std::vector<AssetKey> WaitingParents;
        bool bCancelRequested = false;
    };

    struct CompletionEvent
    {
        AssetKey Key{};
        uint64_t Generation = 0;
        AssetLoadState State = AssetLoadState::Failed;
        std::shared_ptr<const void> Resource;
        AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
    };

    void RequestLoad(const AssetKey& Key, AssetType ExpectedType);
    void BeginLoadWhenReady(AssetSlot& Slot);
    void ScheduleWorkerLoad(AssetSlot& Slot, const AssetLoadContext& Context, IAssetLoader* Loader);
    void PublishCompletion(const CompletionEvent& Event);
    void FailSlot(AssetSlot& Slot, const AssetDiagnostic& Diagnostic);
    void NotifyParents(const AssetKey& ChildKey);
    bool HasDependencyCycle(const AssetKey& RootKey, const std::vector<AssetKey>& Dependencies) const;
    IAssetLoader* ResolveLoader(AssetType Type);
    AssetType ResolveExpectedType(const AssetKey& Key) const;
    AssetSlot* FindSlot(const AssetKey& Key);
    const AssetSlot* FindSlot(const AssetKey& Key) const;

    template <typename ResourceType>
    static AssetType ResourceAssetType();

    AssetRegistry* Registry = nullptr;
    JobSystem* Jobs = nullptr;
    bool bInitialized = false;

    ModelLoader ModelLoaderInstance;
    TextureLoader TextureLoaderInstance;
    MaterialLoader MaterialLoaderInstance;
    SkeletalLoader SkeletalLoaderInstance;

    mutable std::mutex SlotsMutex;
    std::unordered_map<AssetKey, AssetSlot, AssetKeyHash> Slots;
    std::unordered_map<AssetKey, uint64_t, AssetKeyHash> Generations;

    std::mutex CompletionMutex;
    std::queue<CompletionEvent> Completions;
};

template <typename ResourceType>
AssetType AssetManager::ResourceAssetType()
{
    if constexpr (std::is_same_v<ResourceType, ModelResource>)
    {
        return AssetType::Model;
    }
    else if constexpr (std::is_same_v<ResourceType, TextureResource>)
    {
        return AssetType::Texture;
    }
    else if constexpr (std::is_same_v<ResourceType, MaterialResource>)
    {
        return AssetType::Material;
    }
    else if constexpr (std::is_same_v<ResourceType, StaticMeshResource>)
    {
        return AssetType::StaticMesh;
    }
    else if constexpr (std::is_same_v<ResourceType, SkeletalMeshResource>)
    {
        return AssetType::SkeletalMesh;
    }
    else if constexpr (std::is_same_v<ResourceType, SkeletonResource>)
    {
        return AssetType::Skeleton;
    }
    else if constexpr (std::is_same_v<ResourceType, AnimationClipResource>)
    {
        return AssetType::AnimationClip;
    }
    else
    {
        return AssetType::Unknown;
    }
}

template <typename ResourceType>
void AssetManager::LoadAsync(const AssetKey& Key)
{
    RequestLoad(Key, ResourceAssetType<ResourceType>());
}

template <typename ResourceType>
bool AssetManager::TryGetLoaded(const AssetKey& Key, AssetHandle<ResourceType>& OutHandle) const
{
    std::lock_guard<std::mutex> Lock(SlotsMutex);
    const AssetSlot* Slot = FindSlot(Key);
    if (Slot == nullptr || Slot->State != AssetLoadState::Ready || Slot->Resource == nullptr)
    {
        return false;
    }

    if (Slot->Type != ResourceAssetType<ResourceType>() && ResourceAssetType<ResourceType>() != AssetType::Unknown)
    {
        if constexpr (std::is_same_v<ResourceType, StaticMeshResource>)
        {
            if (Slot->Type != AssetType::StaticMesh)
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    OutHandle = AssetHandle<ResourceType>(std::static_pointer_cast<const ResourceType>(Slot->Resource));
    return true;
}
