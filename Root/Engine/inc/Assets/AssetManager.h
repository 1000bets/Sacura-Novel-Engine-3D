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

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
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
    ~AssetManager();

    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;

    void Initialize(AssetRegistry& Registry, JobSystem& Jobs);
    void Shutdown();

    template <typename ResourceType>
    void LoadAsync(const AssetKey& Key);

    template <typename ResourceType>
    bool TryGetLoaded(const AssetKey& Key, AssetHandle<ResourceType>& OutHandle) const;

    AssetLoadState GetLoadState(const AssetKey& Key) const;
    AssetDiagnostic GetLastDiagnostic(const AssetKey& Key) const;

    void CancelLoad(const AssetKey& Key);
    void PumpCompletions();
    void UnloadUnused();

    uint64_t GetSessionId() const { return SessionId.load(std::memory_order_acquire); }

    // Test seam: when set, worker jobs spin until the flag becomes true before calling Load.
    void SetWorkerLoadReleaseFlag(std::shared_ptr<std::atomic<bool>> ReleaseFlag);
    void ClearWorkerLoadReleaseFlag();

    // Test seam: invoked on the worker before Load; may throw to simulate loader faults.
    void SetWorkerLoadFault(std::function<void()> Fault);
    void ClearWorkerLoadFault();

private:
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
        std::shared_ptr<std::atomic<bool>> CancelFlag = std::make_shared<std::atomic<bool>>(false);
    };

    struct CompletionEvent
    {
        AssetKey Key{};
        uint64_t Generation = 0;
        uint64_t SessionId = 0;
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
    void ResetOutstandingLoadsGroup();

    template <typename ResourceType>
    static AssetType ResourceAssetType();

    AssetRegistry* Registry = nullptr;
    JobSystem* Jobs = nullptr;
    bool bInitialized = false;
    std::atomic<bool> bAcceptingLoads{false};
    std::atomic<uint64_t> SessionId{0};

    ModelLoader ModelLoaderInstance;
    TextureLoader TextureLoaderInstance;
    MaterialLoader MaterialLoaderInstance;
    SkeletalLoader SkeletalLoaderInstance;

    mutable std::mutex SlotsMutex;
    std::unordered_map<AssetKey, AssetSlot, AssetKeyHash> Slots;
    std::unordered_map<AssetKey, uint64_t, AssetKeyHash> Generations;

    std::mutex CompletionMutex;
    std::queue<CompletionEvent> Completions;

    JobGroup OutstandingLoads;
    std::shared_ptr<std::atomic<bool>> SessionCancelFlag;

    std::shared_ptr<std::atomic<bool>> WorkerLoadReleaseFlag;
    std::function<void()> WorkerLoadFault;
    std::mutex WorkerLoadFaultMutex;
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
