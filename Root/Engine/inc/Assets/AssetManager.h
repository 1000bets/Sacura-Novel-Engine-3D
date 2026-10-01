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
#include "Assets/Resources/SkinBinding.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Core/Threading/JobSystem.h"
#include "Core/Threading/ThreadContext.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <type_traits>
#include <typeindex>
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
    AssetDiagnostic RegisterLoader(const AssetType& Type, IAssetLoader& Loader);

    template <typename ResourceType>
    AssetDiagnostic RegisterLoader(const AssetType& Type, IAssetLoader& Loader);

    template <typename ResourceType>
    void LoadAsync(const AssetKey& Key);

    template <typename ResourceType>
    bool TryGetLoaded(const AssetKey& Key, AssetHandle<ResourceType>& OutHandle) const;

    AssetLoadState GetLoadState(const AssetKey& Key) const;
    AssetDiagnostic GetLastDiagnostic(const AssetKey& Key) const;

    void CancelLoad(const AssetKey& Key);
    void InvalidateAsset(const AssetId& Id);
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
        AssetType Type = UnknownAssetType;
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
    AssetType ResourceAssetType() const;

    AssetRegistry* Registry = nullptr;
    JobSystem* Jobs = nullptr;
    bool bInitialized = false;
    std::atomic<bool> bAcceptingLoads{false};
    std::atomic<uint64_t> SessionId{0};

    ModelLoader ModelLoaderInstance;
    TextureLoader TextureLoaderInstance;
    MaterialLoader MaterialLoaderInstance;
    MaterialInstanceLoader MaterialInstanceLoaderInstance;
    SkeletalLoader SkeletalLoaderInstance;
    std::unordered_map<AssetType, IAssetLoader*, AssetTypeHash> LoadersByType;
    std::unordered_map<std::type_index, AssetType> CustomResourceTypes;

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
AssetType AssetManager::ResourceAssetType() const
{
    if constexpr (std::is_same_v<ResourceType, ModelResource>)
    {
        return ModelAssetType;
    }
    else if constexpr (std::is_same_v<ResourceType, TextureResource>)
    {
        return TextureAssetType;
    }
    else if constexpr (std::is_same_v<ResourceType, MaterialResource>)
    {
        return MaterialAssetType;
    }
    else if constexpr (std::is_same_v<ResourceType, MaterialInstanceResource>)
    {
        return MaterialInstanceAssetType;
    }
    else if constexpr (std::is_same_v<ResourceType, StaticMeshResource>)
    {
        return StaticMeshAssetType;
    }
    else if constexpr (std::is_same_v<ResourceType, SkeletalMeshResource>)
    {
        return SkeletalMeshAssetType;
    }
    else if constexpr (std::is_same_v<ResourceType, SkeletonResource>)
    {
        return SkeletonAssetType;
    }
    else if constexpr (std::is_same_v<ResourceType, AnimationClipResource>)
    {
        return AnimationClipAssetType;
    }
    else if constexpr (std::is_same_v<ResourceType, SkinBinding>)
    {
        return SkinBindingAssetType;
    }
    else
    {
        const auto Registered = CustomResourceTypes.find(std::type_index(typeid(ResourceType)));
        if (Registered != CustomResourceTypes.end())
        {
            return Registered->second;
        }
        return UnknownAssetType;
    }
}

template <typename ResourceType>
AssetDiagnostic AssetManager::RegisterLoader(const AssetType& Type, IAssetLoader& Loader)
{
    const AssetType ExistingType = ResourceAssetType<ResourceType>();
    if (ExistingType.IsValid() && ExistingType != Type)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetManager", "Resource type is already bound to a different asset type");
    }
    for (const auto& Registered : CustomResourceTypes)
    {
        if (Registered.second == Type && Registered.first != std::type_index(typeid(ResourceType)))
        {
            return AssetDiagnostic::Fail(AssetErrorCode::ImportConflict, "AssetManager", "Asset type is already bound to a different resource type");
        }
    }
    const AssetDiagnostic Result = RegisterLoader(Type, Loader);
    if (!Result.HasError())
    {
        CustomResourceTypes.emplace(std::type_index(typeid(ResourceType)), Type);
    }
    return Result;
}

template <typename ResourceType>
void AssetManager::LoadAsync(const AssetKey& Key)
{
    const AssetType ExpectedType = ResourceAssetType<ResourceType>();
    if (!ExpectedType.IsValid())
    {
        PrintString("AssetManager: resource type requires typed loader registration");
        return;
    }
    RequestLoad(Key, ExpectedType);
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

    const AssetType ExpectedType = ResourceAssetType<ResourceType>();
    if (!ExpectedType.IsValid() || Slot->Type != ExpectedType)
    {
        OutHandle = {};
        return false;
    }

    OutHandle = AssetHandle<ResourceType>(std::static_pointer_cast<const ResourceType>(Slot->Resource));
    return true;
}
