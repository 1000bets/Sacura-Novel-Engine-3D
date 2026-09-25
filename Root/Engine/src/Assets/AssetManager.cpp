#include "Assets/AssetManager.h"
#include "Core/Threading/ThreadContext.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <functional>
#include <string>
#include <thread>
#include <utility>

AssetManager::AssetManager()
    : SkeletalLoaderInstance(ModelLoaderInstance)
{
    RegisterLoader(ModelAssetType, ModelLoaderInstance);
    RegisterLoader(StaticMeshAssetType, ModelLoaderInstance);
    RegisterLoader(TextureAssetType, TextureLoaderInstance);
    RegisterLoader(MaterialAssetType, MaterialLoaderInstance);
    RegisterLoader(SkeletalMeshAssetType, SkeletalLoaderInstance);
    RegisterLoader(SkeletonAssetType, SkeletalLoaderInstance);
    RegisterLoader(AnimationClipAssetType, SkeletalLoaderInstance);
    RegisterLoader(SkinBindingAssetType, SkeletalLoaderInstance);
}

AssetManager::~AssetManager()
{
    if (bInitialized)
    {
        Shutdown();
    }
}

void AssetManager::SetWorkerLoadReleaseFlag(std::shared_ptr<std::atomic<bool>> ReleaseFlag)
{
    WorkerLoadReleaseFlag = std::move(ReleaseFlag);
}

void AssetManager::ClearWorkerLoadReleaseFlag()
{
    WorkerLoadReleaseFlag.reset();
}

void AssetManager::SetWorkerLoadFault(std::function<void()> Fault)
{
    std::lock_guard<std::mutex> Lock(WorkerLoadFaultMutex);
    WorkerLoadFault = std::move(Fault);
}

void AssetManager::ClearWorkerLoadFault()
{
    std::lock_guard<std::mutex> Lock(WorkerLoadFaultMutex);
    WorkerLoadFault = nullptr;
}

AssetDiagnostic AssetManager::RegisterLoader(const AssetType& Type, IAssetLoader& Loader)
{
    if (!Type.IsValid())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::InvalidData,
            "AssetManager",
            "Cannot register a loader for an empty asset type");
    }
    if (LoadersByType.find(Type) != LoadersByType.end())
    {
        return AssetDiagnostic::Fail(
            AssetErrorCode::ImportConflict,
            "AssetManager",
            "Loader is already registered for asset type " + Type.GetIdentifier());
    }
    LoadersByType.emplace(Type, &Loader);
    return AssetDiagnostic::Ok();
}

void AssetManager::ResetOutstandingLoadsGroup()
{
    OutstandingLoads = JobGroup{};
}

void AssetManager::Initialize(AssetRegistry& InRegistry, JobSystem& InJobs)
{
    AssertGameThread();
    if (bInitialized)
    {
        Shutdown();
    }

    Registry = &InRegistry;
    Jobs = &InJobs;
    SessionCancelFlag = std::make_shared<std::atomic<bool>>(false);
    ResetOutstandingLoadsGroup();
    SessionId.fetch_add(1, std::memory_order_acq_rel);
    bAcceptingLoads.store(true, std::memory_order_release);
    bInitialized = true;
    PrintString("AssetManager: initialized");
}

void AssetManager::Shutdown()
{
    AssertGameThread();
    if (!bInitialized)
    {
        return;
    }

    bAcceptingLoads.store(false, std::memory_order_release);
    if (SessionCancelFlag)
    {
        SessionCancelFlag->store(true, std::memory_order_release);
    }

    {
        std::lock_guard<std::mutex> Lock(SlotsMutex);
        for (auto& Pair : Slots)
        {
            Pair.second.bCancelRequested = true;
            if (Pair.second.CancelFlag)
            {
                Pair.second.CancelFlag->store(true, std::memory_order_release);
            }
            if (Pair.second.State == AssetLoadState::Queued || Pair.second.State == AssetLoadState::Loading)
            {
                Pair.second.State = AssetLoadState::Cancelled;
                Pair.second.Diagnostic = AssetDiagnostic::Fail(
                    AssetErrorCode::Cancelled,
                    "AssetManager",
                    "Load cancelled by shutdown",
                    Pair.second.Key);
            }
        }
    }

    OutstandingLoads.Wait();
    ResetOutstandingLoadsGroup();

    PumpCompletions();

    {
        std::lock_guard<std::mutex> Lock(SlotsMutex);
        Slots.clear();
        Generations.clear();
    }

    {
        std::lock_guard<std::mutex> Lock(CompletionMutex);
        while (!Completions.empty())
        {
            Completions.pop();
        }
    }

    ModelLoaderInstance.ClearSharedDocuments();
    ClearWorkerLoadReleaseFlag();
    ClearWorkerLoadFault();
    SessionCancelFlag.reset();
    SessionId.fetch_add(1, std::memory_order_acq_rel);
    Registry = nullptr;
    Jobs = nullptr;
    bInitialized = false;
    PrintString("AssetManager: shutdown");
}

void AssetManager::CancelLoad(const AssetKey& Key)
{
    AssertGameThread();
    std::lock_guard<std::mutex> Lock(SlotsMutex);
    AssetSlot* Slot = FindSlot(Key);
    if (Slot == nullptr)
    {
        return;
    }

    Slot->bCancelRequested = true;
    if (Slot->CancelFlag)
    {
        Slot->CancelFlag->store(true, std::memory_order_release);
    }

    if (Slot->State == AssetLoadState::Queued)
    {
        Slot->State = AssetLoadState::Cancelled;
        Slot->Diagnostic = AssetDiagnostic::Fail(AssetErrorCode::Cancelled, "AssetManager", "Load cancelled", Key);
        Slot->Resource.reset();
        NotifyParents(Key);
    }
}

void AssetManager::InvalidateAsset(const AssetId& Id)
{
    AssertGameThread();
    std::lock_guard<std::mutex> Lock(SlotsMutex);
    for (auto Iterator = Slots.begin(); Iterator != Slots.end();)
    {
        if (Iterator->first.Asset != Id)
        {
            ++Iterator;
            continue;
        }
        if (Iterator->second.CancelFlag)
        {
            Iterator->second.CancelFlag->store(true, std::memory_order_release);
        }
        Iterator = Slots.erase(Iterator);
    }
}

AssetManager::AssetSlot* AssetManager::FindSlot(const AssetKey& Key)
{
    auto Iterator = Slots.find(Key);
    if (Iterator == Slots.end())
    {
        return nullptr;
    }
    return &Iterator->second;
}

const AssetManager::AssetSlot* AssetManager::FindSlot(const AssetKey& Key) const
{
    auto Iterator = Slots.find(Key);
    if (Iterator == Slots.end())
    {
        return nullptr;
    }
    return &Iterator->second;
}

AssetLoadState AssetManager::GetLoadState(const AssetKey& Key) const
{
    std::lock_guard<std::mutex> Lock(SlotsMutex);
    const AssetSlot* Slot = FindSlot(Key);
    if (Slot == nullptr)
    {
        return AssetLoadState::Unloaded;
    }
    return Slot->State;
}

AssetDiagnostic AssetManager::GetLastDiagnostic(const AssetKey& Key) const
{
    std::lock_guard<std::mutex> Lock(SlotsMutex);
    const AssetSlot* Slot = FindSlot(Key);
    if (Slot == nullptr)
    {
        return AssetDiagnostic::Ok();
    }
    return Slot->Diagnostic;
}

IAssetLoader* AssetManager::ResolveLoader(AssetType Type)
{
    auto Found = LoadersByType.find(Type);
    return Found != LoadersByType.end() ? Found->second : nullptr;
}

AssetType AssetManager::ResolveExpectedType(const AssetKey& Key) const
{
    if (Registry == nullptr)
    {
        return UnknownAssetType;
    }

    AssetRegistryEntry Entry{};
    const SubAssetRecord* SubAsset = nullptr;
    if (!Registry->TryResolveKey(Key, Entry, &SubAsset))
    {
        return UnknownAssetType;
    }

    if (SubAsset != nullptr)
    {
        return SubAsset->Type;
    }

    return Entry.Metadata.Type;
}

bool AssetManager::HasDependencyCycle(const AssetKey& RootKey, const std::vector<AssetKey>& Dependencies) const
{
    std::unordered_map<AssetKey, bool, AssetKeyHash> Visiting;
    std::unordered_map<AssetKey, bool, AssetKeyHash> Visited;

    std::function<bool(const AssetKey&)> Visit = [&](const AssetKey& Current) -> bool
    {
        if (Visiting[Current])
        {
            return true;
        }
        if (Visited[Current])
        {
            return false;
        }

        Visiting[Current] = true;
        const AssetSlot* Slot = FindSlot(Current);
        const std::vector<AssetKey>* NextDependencies = nullptr;
        std::vector<AssetKey> Collected;
        if (Slot != nullptr && !Slot->PendingDependencies.empty())
        {
            NextDependencies = &Slot->PendingDependencies;
        }
        else if (Current == RootKey)
        {
            NextDependencies = &Dependencies;
        }
        else if (Registry != nullptr)
        {
            AssetRegistryEntry Entry{};
            const SubAssetRecord* SubAsset = nullptr;
            if (Registry->TryResolveKey(Current, Entry, &SubAsset))
            {
                AssetLoadContext Context{};
                Context.Registry = Registry;
                Context.Entry = Entry;
                BindLoadContextSubAsset(Context, SubAsset);
                Context.Key = Current;
                IAssetLoader* Loader = const_cast<AssetManager*>(this)->ResolveLoader(
                    Context.SubAsset.has_value() ? Context.SubAsset->Type : Entry.Metadata.Type);
                if (Loader != nullptr)
                {
                    Loader->CollectDependencies(Context, Collected);
                    NextDependencies = &Collected;
                }
            }
        }

        if (NextDependencies != nullptr)
        {
            for (const AssetKey& Dependency : *NextDependencies)
            {
                if (Visit(Dependency))
                {
                    return true;
                }
            }
        }

        Visiting[Current] = false;
        Visited[Current] = true;
        return false;
    };

    return Visit(RootKey);
}

void AssetManager::FailSlot(AssetSlot& Slot, const AssetDiagnostic& Diagnostic)
{
    Slot.State = AssetLoadState::Failed;
    Slot.Diagnostic = Diagnostic;
    Slot.Resource.reset();
}

void AssetManager::NotifyParents(const AssetKey& ChildKey)
{
    AssetSlot* Child = FindSlot(ChildKey);
    if (Child == nullptr)
    {
        return;
    }

    std::vector<AssetKey> Parents = Child->WaitingParents;
    Child->WaitingParents.clear();
    for (const AssetKey& ParentKey : Parents)
    {
        AssetSlot* Parent = FindSlot(ParentKey);
        if (Parent == nullptr)
        {
            continue;
        }

        if (Child->State == AssetLoadState::Failed || Child->State == AssetLoadState::Cancelled)
        {
            FailSlot(
                *Parent,
                AssetDiagnostic::Fail(
                    AssetErrorCode::DependencyFailed,
                    "AssetManager",
                    "Mandatory dependency failed",
                    ParentKey,
                    {},
                    "AssetManager"));
            NotifyParents(ParentKey);
            continue;
        }

        BeginLoadWhenReady(*Parent);
    }
}

void AssetManager::ScheduleWorkerLoad(AssetSlot& Slot, const AssetLoadContext& Context, IAssetLoader* Loader)
{
    if (Jobs == nullptr || Loader == nullptr)
    {
        FailSlot(
            Slot,
            AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetManager", "JobSystem or loader missing", Slot.Key));
        return;
    }

    if (!bAcceptingLoads.load(std::memory_order_acquire))
    {
        FailSlot(
            Slot,
            AssetDiagnostic::Fail(AssetErrorCode::Cancelled, "AssetManager", "Loads are no longer accepted", Slot.Key));
        return;
    }

    if (!Slot.CancelFlag)
    {
        Slot.CancelFlag = std::make_shared<std::atomic<bool>>(false);
    }
    Slot.CancelFlag->store(Slot.bCancelRequested, std::memory_order_release);

    Slot.State = AssetLoadState::Loading;
    const AssetKey Key = Slot.Key;
    const uint64_t Generation = Slot.Generation;
    const uint64_t CapturedSession = SessionId.load(std::memory_order_acquire);
    std::shared_ptr<std::atomic<bool>> CancelFlag = Slot.CancelFlag;
    std::shared_ptr<std::atomic<bool>> ReleaseFlag = WorkerLoadReleaseFlag;
    std::function<void()> Fault;
    {
        std::lock_guard<std::mutex> Lock(WorkerLoadFaultMutex);
        Fault = WorkerLoadFault;
    }

    Jobs->Schedule(OutstandingLoads, [this, Key, Generation, CapturedSession, Context, Loader, CancelFlag, ReleaseFlag, Fault]()
    {
        auto PushEvent = [this](CompletionEvent Event)
        {
            std::lock_guard<std::mutex> Lock(CompletionMutex);
            Completions.push(std::move(Event));
        };

        CompletionEvent Event{};
        Event.Key = Key;
        Event.Generation = Generation;
        Event.SessionId = CapturedSession;

        if (ReleaseFlag)
        {
            while (!ReleaseFlag->load(std::memory_order_acquire))
            {
                if (CancelFlag && CancelFlag->load(std::memory_order_acquire))
                {
                    Event.State = AssetLoadState::Cancelled;
                    Event.Diagnostic = AssetDiagnostic::Fail(AssetErrorCode::Cancelled, "AssetManager", "Load cancelled", Key);
                    PushEvent(std::move(Event));
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

        if ((CancelFlag && CancelFlag->load(std::memory_order_acquire))
            || CapturedSession != SessionId.load(std::memory_order_acquire))
        {
            Event.State = AssetLoadState::Cancelled;
            Event.Diagnostic = AssetDiagnostic::Fail(AssetErrorCode::Cancelled, "AssetManager", "Load cancelled", Key);
            PushEvent(std::move(Event));
            return;
        }

        try
        {
            if (Fault)
            {
                Fault();
            }

            AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
            std::shared_ptr<const void> Resource = Loader->Load(Context, Diagnostic);
            if ((CancelFlag && CancelFlag->load(std::memory_order_acquire))
                || CapturedSession != SessionId.load(std::memory_order_acquire))
            {
                Event.State = AssetLoadState::Cancelled;
                Event.Diagnostic = AssetDiagnostic::Fail(AssetErrorCode::Cancelled, "AssetManager", "Load cancelled", Key);
            }
            else if (!Resource || Diagnostic.HasError())
            {
                Event.State = AssetLoadState::Failed;
                Event.Diagnostic = Diagnostic.HasError()
                    ? Diagnostic
                    : AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetManager", "Loader returned null", Key);
            }
            else
            {
                Event.State = AssetLoadState::Ready;
                Event.Resource = std::move(Resource);
                Event.Diagnostic = AssetDiagnostic::Ok();
            }
        }
        catch (const std::exception& Exception)
        {
            Event.State = AssetLoadState::Failed;
            Event.Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::InternalError,
                "AssetManager",
                std::string("Loader exception: ") + Exception.what(),
                Key);
        }
        catch (...)
        {
            Event.State = AssetLoadState::Failed;
            Event.Diagnostic = AssetDiagnostic::Fail(
                AssetErrorCode::InternalError,
                "AssetManager",
                "Loader unknown exception",
                Key);
        }

        PushEvent(std::move(Event));
    });
}

void AssetManager::BeginLoadWhenReady(AssetSlot& Slot)
{
    if (Slot.State == AssetLoadState::Ready || Slot.State == AssetLoadState::Failed || Slot.State == AssetLoadState::Cancelled || Slot.State == AssetLoadState::Loading)
    {
        return;
    }

    bool bDependenciesReady = true;
    for (const AssetKey& Dependency : Slot.PendingDependencies)
    {
        AssetSlot* DependencySlot = FindSlot(Dependency);
        if (DependencySlot == nullptr || DependencySlot->State == AssetLoadState::Unloaded || DependencySlot->State == AssetLoadState::Queued)
        {
            bDependenciesReady = false;
            if (DependencySlot != nullptr)
            {
                if (std::find(DependencySlot->WaitingParents.begin(), DependencySlot->WaitingParents.end(), Slot.Key) == DependencySlot->WaitingParents.end())
                {
                    DependencySlot->WaitingParents.push_back(Slot.Key);
                }
            }
            continue;
        }

        if (DependencySlot->State == AssetLoadState::Loading)
        {
            bDependenciesReady = false;
            if (std::find(DependencySlot->WaitingParents.begin(), DependencySlot->WaitingParents.end(), Slot.Key) == DependencySlot->WaitingParents.end())
            {
                DependencySlot->WaitingParents.push_back(Slot.Key);
            }
            continue;
        }

        if (DependencySlot->State == AssetLoadState::Failed || DependencySlot->State == AssetLoadState::Cancelled)
        {
            FailSlot(
                Slot,
                AssetDiagnostic::Fail(
                    AssetErrorCode::DependencyFailed,
                    "AssetManager",
                    "Mandatory dependency failed",
                    Slot.Key));
            NotifyParents(Slot.Key);
            return;
        }
    }

    if (!bDependenciesReady)
    {
        Slot.State = AssetLoadState::Queued;
        return;
    }

    AssetRegistryEntry Entry{};
    const SubAssetRecord* SubAsset = nullptr;
    if (Registry == nullptr || !Registry->TryResolveKey(Slot.Key, Entry, &SubAsset))
    {
        FailSlot(Slot, AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetManager", "Asset not registered", Slot.Key));
        NotifyParents(Slot.Key);
        return;
    }

    AssetType Type = SubAsset != nullptr ? SubAsset->Type : Entry.Metadata.Type;
    if (Slot.Type.IsValid() && Slot.Type != Type && !(Slot.Type == StaticMeshAssetType && Type == ModelAssetType))
    {
        if (!(Type == StaticMeshAssetType && Slot.Type == StaticMeshAssetType))
        {
            FailSlot(Slot, AssetDiagnostic::Fail(AssetErrorCode::TypeMismatch, "AssetManager", "Requested type mismatch", Slot.Key, Entry.RelativePath));
            NotifyParents(Slot.Key);
            return;
        }
    }

    Slot.Type = Type;
    IAssetLoader* Loader = ResolveLoader(Type);
    if (Loader == nullptr && Type == StaticMeshAssetType)
    {
        Loader = &ModelLoaderInstance;
    }

    if (Loader == nullptr)
    {
        FailSlot(Slot, AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFormat, "AssetManager", "No loader for asset type", Slot.Key, Entry.RelativePath));
        NotifyParents(Slot.Key);
        return;
    }

    AssetLoadContext Context{};
    Context.Registry = Registry;
    Context.Entry = Entry;
    BindLoadContextSubAsset(Context, SubAsset);
    Context.Key = Slot.Key;
    ScheduleWorkerLoad(Slot, Context, Loader);
}

void AssetManager::RequestLoad(const AssetKey& Key, AssetType ExpectedType)
{
    AssertGameThread();
    if (!bInitialized || !bAcceptingLoads.load(std::memory_order_acquire) || !Key.IsValid())
    {
        return;
    }

    std::vector<AssetKey> DependenciesToRequest;
    {
        std::lock_guard<std::mutex> Lock(SlotsMutex);
        AssetSlot* Existing = FindSlot(Key);
        if (Existing != nullptr)
        {
            if (Existing->State == AssetLoadState::Ready
                || Existing->State == AssetLoadState::Queued
                || Existing->State == AssetLoadState::Loading)
            {
                return;
            }

            if (Existing->State == AssetLoadState::Failed || Existing->State == AssetLoadState::Cancelled)
            {
                Generations[Key] = Existing->Generation + 1;
                Existing->Generation = Generations[Key];
                Existing->State = AssetLoadState::Queued;
                Existing->Resource.reset();
                Existing->Diagnostic = AssetDiagnostic::Ok();
                Existing->PendingDependencies.clear();
                Existing->bCancelRequested = false;
                Existing->Type = ExpectedType;
            }
        }
        else
        {
            uint64_t Generation = 1;
            auto GenerationIterator = Generations.find(Key);
            if (GenerationIterator != Generations.end())
            {
                Generation = GenerationIterator->second + 1;
            }
            Generations[Key] = Generation;

            AssetSlot Slot{};
            Slot.Key = Key;
            Slot.Generation = Generation;
            Slot.State = AssetLoadState::Queued;
            Slot.Type = ExpectedType;
            Slots.emplace(Key, std::move(Slot));
        }

        AssetSlot& Slot = *FindSlot(Key);
        AssetRegistryEntry Entry{};
        const SubAssetRecord* SubAsset = nullptr;
        if (Registry == nullptr || !Registry->TryResolveKey(Key, Entry, &SubAsset))
        {
            FailSlot(Slot, AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetManager", "Asset not registered", Key));
            return;
        }

        const AssetType ResolvedType = SubAsset != nullptr ? SubAsset->Type : Entry.Metadata.Type;
        Slot.Type = ExpectedType.IsValid() ? ExpectedType : ResolvedType;

        AssetLoadContext Context{};
        Context.Registry = Registry;
        Context.Entry = Entry;
        BindLoadContextSubAsset(Context, SubAsset);
        Context.Key = Key;

        IAssetLoader* Loader = ResolveLoader(Slot.Type);
        if (Loader == nullptr && Slot.Type == StaticMeshAssetType)
        {
            Loader = &ModelLoaderInstance;
        }

        Slot.PendingDependencies.clear();
        if (Loader != nullptr)
        {
            Loader->CollectDependencies(Context, Slot.PendingDependencies);
        }

        if (HasDependencyCycle(Key, Slot.PendingDependencies))
        {
            FailSlot(Slot, AssetDiagnostic::Fail(AssetErrorCode::DependencyCycle, "AssetManager", "Dependency cycle detected", Key, Entry.RelativePath));
            return;
        }

        DependenciesToRequest = Slot.PendingDependencies;
    }

    for (const AssetKey& Dependency : DependenciesToRequest)
    {
        RequestLoad(Dependency, ResolveExpectedType(Dependency));
    }

    {
        std::lock_guard<std::mutex> Lock(SlotsMutex);
        AssetSlot* Slot = FindSlot(Key);
        if (Slot == nullptr)
        {
            return;
        }

        for (const AssetKey& Dependency : Slot->PendingDependencies)
        {
            AssetSlot* DependencySlot = FindSlot(Dependency);
            if (DependencySlot != nullptr)
            {
                if (std::find(DependencySlot->WaitingParents.begin(), DependencySlot->WaitingParents.end(), Key) == DependencySlot->WaitingParents.end())
                {
                    DependencySlot->WaitingParents.push_back(Key);
                }
            }
        }

        BeginLoadWhenReady(*Slot);
    }
}

void AssetManager::PublishCompletion(const CompletionEvent& Event)
{
    AssertGameThread();
    if (Event.SessionId != SessionId.load(std::memory_order_acquire))
    {
        return;
    }

    AssetSlot* Slot = FindSlot(Event.Key);
    if (Slot == nullptr || Slot->Generation != Event.Generation)
    {
        return;
    }

    if (Slot->bCancelRequested || (Slot->CancelFlag && Slot->CancelFlag->load(std::memory_order_acquire)))
    {
        Slot->State = AssetLoadState::Cancelled;
        Slot->Diagnostic = AssetDiagnostic::Fail(AssetErrorCode::Cancelled, "AssetManager", "Load cancelled", Event.Key);
        Slot->Resource.reset();
    }
    else
    {
        Slot->State = Event.State;
        Slot->Resource = Event.Resource;
        Slot->Diagnostic = Event.Diagnostic;
    }

    NotifyParents(Event.Key);
}

void AssetManager::PumpCompletions()
{
    AssertGameThread();

    std::queue<CompletionEvent> LocalCompletions;
    {
        std::lock_guard<std::mutex> Lock(CompletionMutex);
        std::swap(LocalCompletions, Completions);
    }

    std::lock_guard<std::mutex> Lock(SlotsMutex);
    while (!LocalCompletions.empty())
    {
        PublishCompletion(LocalCompletions.front());
        LocalCompletions.pop();
    }
}

void AssetManager::UnloadUnused()
{
    AssertGameThread();
    std::lock_guard<std::mutex> Lock(SlotsMutex);
    for (auto Iterator = Slots.begin(); Iterator != Slots.end();)
    {
        AssetSlot& Slot = Iterator->second;
        if (Slot.State == AssetLoadState::Ready && Slot.Resource && Slot.Resource.use_count() == 1)
        {
            Iterator = Slots.erase(Iterator);
            continue;
        }

        if (Slot.State == AssetLoadState::Failed || Slot.State == AssetLoadState::Cancelled)
        {
            Iterator = Slots.erase(Iterator);
            continue;
        }

        ++Iterator;
    }
}
