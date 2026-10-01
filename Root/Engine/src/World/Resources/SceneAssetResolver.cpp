#include "World/Resources/SceneAssetResolver.h"

#include "Assets/Resources/MaterialResource.h"
#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/TextureResource.h"
#include "Core/Threading/ThreadContext.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/Components/CameraComponent.h"
#include <algorithm>

void SceneAssetResolver::Bind(
    AssetRegistry* Registry,
    AssetManager* Manager,
    AssetGpuUploader* Uploader)
{
    BoundRegistry = Registry;
    BoundManager = Manager;
    BoundUploader = Uploader;
}

void SceneAssetResolver::ResolveScene(Scene& TargetScene)
{
    AssertGameThread();
    if (BoundManager == nullptr || BoundUploader == nullptr)
    {
        return;
    }

    for (GameObject* ObjectInstance : TargetScene.GetAllObjects())
    {
        if (ObjectInstance == nullptr)
        {
            continue;
        }

        for (Component* ComponentInstance : ObjectInstance->GetAllComponents())
        {
            if (auto* Camera = dynamic_cast<CameraComponent*>(ComponentInstance))
            {
                Camera->MaterialDiagnostic.clear();
                for (auto& Effect : Camera->PostProcessEffects.Entries)
                {
                    if (!Effect.Material.IsValid() && !Effect.DynamicMaterial)
                    {
                        Effect.Snapshot.reset();
                    }
                    if (Effect.bEnabled)
                    {
                        const auto Snapshot = ResolveMaterial(Effect.Material, Effect.DynamicMaterial);
                        const auto Diagnostic = GetMaterialDiagnostic(Effect.Material);
                        if (!Diagnostic.empty())
                        {
                            Camera->MaterialDiagnostic += Diagnostic + "\n";
                        }
                        if (Snapshot && Snapshot->Material->Definition.Domain != MaterialDomain::PostProcess)
                        {
                            Camera->MaterialDiagnostic += "Camera effect requires a PostProcess material\n";
                        }
                        if (Snapshot && Snapshot->Material->Definition.Domain == MaterialDomain::PostProcess)
                        {
                            Effect.Snapshot = Snapshot;
                        }
                    }
                }
                continue;
            }
            MeshRendererComponent* MeshRenderer = dynamic_cast<MeshRendererComponent*>(ComponentInstance);
            if (MeshRenderer == nullptr || !MeshRenderer->IsEnabled())
            {
                continue;
            }

            MeshRenderer->MaterialDiagnostic.clear();
            const AssetKey MeshKey = MeshRenderer->GetMeshAssetKey();
            if (!MeshKey.IsValid())
            {
                if (!MeshRenderer->MeshAssetId.empty())
                {
                    if (!MeshRenderer->bMissingAsset)
                    {
                        PrintString("SceneAssetResolver: invalid mesh asset id on '" + ObjectInstance->GetName() + "'");
                    }
                    MeshRenderer->bMissingAsset = true;
                    MeshRenderer->bPendingAsset = false;
                    MeshRenderer->Mesh = MeshHandle{};
                }
                continue;
            }

            if (BoundManager->GetLoadState(MeshKey) == AssetLoadState::Unloaded)
            {
                BoundManager->LoadAsync<StaticMeshResource>(MeshKey);
            }
            const AssetLoadState LoadState = BoundManager->GetLoadState(MeshKey);
            if (LoadState == AssetLoadState::Failed)
            {
                if (!MeshRenderer->bMissingAsset)
                {
                    PrintString(
                        std::string("SceneAssetResolver: mesh load failed for '")
                        + ObjectInstance->GetName()
                        + "': "
                        + BoundManager->GetLastDiagnostic(MeshKey).Message);
                }
                MeshRenderer->bMissingAsset = true;
                MeshRenderer->bPendingAsset = false;
                MeshRenderer->Mesh = MeshHandle{};
                continue;
            }

            AssetHandle<StaticMeshResource> CpuMesh;
            if (!BoundManager->TryGetLoaded(MeshKey, CpuMesh) || !CpuMesh.IsValid())
            {
                MeshRenderer->bPendingAsset = true;
                MeshRenderer->bMissingAsset = false;
                continue;
            }

            BoundUploader->RequestUploadMesh(CpuMesh, MeshKey);
            MeshHandle GpuMesh{};
            if (BoundUploader->TryGetMesh(MeshKey, GpuMesh))
            {
                MeshRenderer->Mesh = GpuMesh;
                MeshRenderer->LocalBounds = CpuMesh->Bounds;
                MeshRenderer->Submeshes = CpuMesh->Submeshes;
                MeshRenderer->bPendingAsset = false;
                MeshRenderer->bMissingAsset = false;
            }
            else if (BoundUploader->GetMeshState(MeshKey) == AssetGpuState::Failed)
            {
                MeshRenderer->bMissingAsset = true;
                MeshRenderer->bPendingAsset = false;
                MeshRenderer->Mesh = MeshHandle{};
                PrintString(
                    std::string("SceneAssetResolver: mesh GPU upload failed for '")
                    + ObjectInstance->GetName()
                    + "'");
            }
            else
            {
                MeshRenderer->bPendingAsset = true;
                MeshRenderer->bMissingAsset = false;
            }

            for (auto Iterator = MeshRenderer->SlotSnapshots.begin(); Iterator != MeshRenderer->SlotSnapshots.end();)
        {
            const bool bAuthored = std::any_of(MeshRenderer->MaterialSlots.Entries.begin(), MeshRenderer->MaterialSlots.Entries.end(),
                [&](const MaterialSlotOverride& Entry) { return Entry.Slot == Iterator->first; });
            if (!bAuthored && MeshRenderer->DynamicMaterialSlots.find(Iterator->first) == MeshRenderer->DynamicMaterialSlots.end())
            {
                Iterator = MeshRenderer->SlotSnapshots.erase(Iterator);
            }
            else
            {
                ++Iterator;
            }
        }
        for (const auto& Slot : MeshRenderer->MaterialSlots.Entries)
        {
            std::shared_ptr<DynamicMaterialInstance> Dynamic;
            const auto Found = MeshRenderer->DynamicMaterialSlots.find(Slot.Slot);
            if (Found != MeshRenderer->DynamicMaterialSlots.end())
            {
                Dynamic = Found->second;
            }
            const auto Snapshot = ResolveMaterial(Slot.Material, Dynamic);
            MeshRenderer->MaterialDiagnostic += GetMaterialDiagnostic(Slot.Material);
            if (Snapshot && Snapshot->Material->Definition.Domain != MaterialDomain::Surface)
            {
                MeshRenderer->MaterialDiagnostic += "Mesh slot requires a Surface material";
            }
            if (Snapshot && Snapshot->Material->Definition.Domain == MaterialDomain::Surface)
            {
                MeshRenderer->SlotSnapshots[Slot.Slot] = Snapshot;
            }
        }
        for (const auto& Slot : MeshRenderer->DynamicMaterialSlots)
        {
            const auto Snapshot = ResolveMaterial({}, Slot.second);
            if (Snapshot && Snapshot->Material->Definition.Domain == MaterialDomain::Surface)
            {
                MeshRenderer->SlotSnapshots[Slot.first] = Snapshot;
            }
        }
        const auto Snapshot = ResolveMaterial(MeshRenderer->GetMaterialAssetKey(), MeshRenderer->DynamicMaterial);
        MeshRenderer->MaterialDiagnostic += GetMaterialDiagnostic(MeshRenderer->GetMaterialAssetKey());
        if (Snapshot && Snapshot->Material->Definition.Domain != MaterialDomain::Surface)
        {
            MeshRenderer->MaterialDiagnostic += "Mesh requires a Surface material";
        }
            if (Snapshot && Snapshot->Material->Definition.Domain == MaterialDomain::Surface)
            {
                MeshRenderer->MaterialSnapshot = Snapshot;
            }
            else if (!MeshRenderer->GetMaterialAssetKey().IsValid() && !MeshRenderer->DynamicMaterial)
            {
                MeshRenderer->MaterialSnapshot.reset();
            }
        }
    }
}

std::shared_ptr<const MaterialRenderSnapshot> SceneAssetResolver::ResolveMaterial(const AssetKey& Key, const std::shared_ptr<DynamicMaterialInstance>& Dynamic)
{
    AssertGameThread();
    auto& Cached = Materials[Key];
    auto ReportDiagnostic = [&](const std::string& Diagnostic)
    {
        if (Cached.Diagnostic != Diagnostic && !Diagnostic.empty())
        {
            PrintString("Material: " + Diagnostic);
        }
        Cached.Diagnostic = Diagnostic;
        if (!Diagnostic.empty() && !Cached.Published)
        {
            for (const auto& Type : {MaterialAssetType, MaterialInstanceAssetType, ShaderSourceAssetType})
            {
                for (const auto& Entry : BoundRegistry->FindByType(Type))
                {
                    if (Cached.Files.find(Entry.AbsolutePath) == Cached.Files.end())
                    {
                        Cached.Files.emplace(Entry.AbsolutePath, ReadFileRevision(Entry.AbsolutePath));
                    }
                }
            }
        }
    };
    CachedDynamicMaterial* DynamicCache = nullptr;
    std::shared_ptr<const MaterialRenderSnapshot> Previous = Cached.Published;
    if (Dynamic)
    {
        auto& Entry = DynamicMaterials[Dynamic.get()];
        if (Entry.Instance.lock() != Dynamic)
        {
            Entry = {};
            Entry.Instance = Dynamic;
        }
        DynamicCache = &Entry;
        Previous = Entry.Published;
    }
    std::shared_ptr<const ResolvedMaterial> Material;
    if (Dynamic)
    {
        const auto& Current = Dynamic->GetResolvedMaterial();
        if (Dynamic->FollowsAssetChanges() && Current.Asset.IsValid())
        {
            const auto Parent = ResolveMaterial(Current.Asset);
            if (Parent && Parent->Material->Definition.Revision != Current.Definition.Revision)
            {
                Dynamic->Rebase(*Parent->Material);
            }
        }
        Material = std::make_shared<const ResolvedMaterial>(Dynamic->GetResolvedMaterial());
    }
    else
    {
        AssetRegistryEntry Entry;
        if (!Key.IsValid() || !BoundRegistry->TryResolveKey(Key, Entry))
        {
            return Previous;
        }
        if (Cached.Files.find(Entry.AbsolutePath) == Cached.Files.end())
        {
            Cached.Files.emplace(Entry.AbsolutePath, ReadFileRevision(Entry.AbsolutePath));
        }
        std::shared_ptr<const MaterialResource> Resource;
        if (Entry.Metadata.Type == MaterialInstanceAssetType)
        {
            BoundManager->LoadAsync<MaterialInstanceResource>(Key);
            AssetHandle<MaterialInstanceResource> Loaded;
            if (!BoundManager->TryGetLoaded(Key, Loaded))
            {
                ReportDiagnostic(BoundManager->GetLastDiagnostic(Key).Message);
                return Previous;
            }
            Resource = Loaded.GetSharedResource();
        }
        else if (Entry.Metadata.Type == MaterialAssetType)
        {
            BoundManager->LoadAsync<MaterialResource>(Key);
            AssetHandle<MaterialResource> Loaded;
            if (!BoundManager->TryGetLoaded(Key, Loaded))
            {
                ReportDiagnostic(BoundManager->GetLastDiagnostic(Key).Message);
                return Previous;
            }
            Resource = Loaded.GetSharedResource();
        }
        else
        {
            return Previous;
        }
        Material = std::shared_ptr<const ResolvedMaterial>(Resource, &Resource->Resolved);
    }
    const auto Compilation = BoundUploader->RequestMaterial(*Material);
    if (!Dynamic)
    {
        for (const auto& Path : Material->Definition.SourceFiles)
        {
            if (Cached.Files.find(Path) == Cached.Files.end())
            {
                Cached.Files.emplace(Path, ReadFileRevision(Path));
            }
        }
        if (Cached.Published && Cached.Published->Material->Definition.Revision == Material->Definition.Revision)
        {
            bool bBindingsCurrent = true;
            size_t TextureIndex = 0;
            for (const auto& Parameter : Material->Definition.Parameters)
            {
                if (Parameter.DefaultValue.Type == MaterialParameterType::Texture2D)
                {
                    const auto& Texture = Material->Values.at(Parameter.Identifier).Texture;
                    TextureHandle Resident;
                    if (Texture.IsValid() && (!BoundUploader->TryGetTexture(Texture, Resident, Parameter.bSrgb)
                        || Resident != Cached.Published->Textures[TextureIndex].Texture))
                    {
                        bBindingsCurrent = false;
                    }
                    ++TextureIndex;
                }
            }
            if (bBindingsCurrent)
            {
                return Previous;
            }
        }
        if (Compilation->State.load() == MaterialCompilationState::Failed)
        {
            ReportDiagnostic(Compilation->GetDiagnostic());
            return Previous;
        }
        if (Cached.Published && Compilation->State.load() != MaterialCompilationState::Ready)
        {
            return Previous;
        }
    }
    if (Dynamic && Compilation->State.load() == MaterialCompilationState::Failed)
    {
        ReportDiagnostic(Compilation->GetDiagnostic());
        return Previous;
    }
    if (Dynamic && Previous && Compilation->State.load() != MaterialCompilationState::Ready)
    {
        return Previous;
    }
    auto Snapshot = std::make_shared<MaterialRenderSnapshot>();
    Snapshot->Material = Material;
    Snapshot->Parameters = MaterialDefinitionIO::PackParameters(*Material);
    for (const auto& Parameter : Material->Definition.Parameters)
    {
        if (Parameter.DefaultValue.Type != MaterialParameterType::Texture2D)
        {
            continue;
        }
        MaterialTextureBinding Binding;
        Binding.Identifier = Parameter.Identifier;
        const AssetKey Texture = Material->Values.at(Parameter.Identifier).Texture;
        if (Texture.IsValid())
        {
            AssetRegistryEntry Entry;
            if (BoundRegistry->TryResolveKey(Texture, Entry) && TextureFiles.find(Entry.AbsolutePath) == TextureFiles.end())
            {
                TextureFiles.emplace(Entry.AbsolutePath, WatchedTexture{Texture, ReadFileRevision(Entry.AbsolutePath), {}, {}, {}, false});
            }
            BoundManager->LoadAsync<TextureResource>(Texture);
            AssetHandle<TextureResource> Loaded;
            if (!BoundManager->TryGetLoaded(Texture, Loaded))
            {
                ReportDiagnostic(BoundManager->GetLastDiagnostic(Texture).Message);
                return Previous;
            }
            BoundUploader->RequestUploadTexture(Loaded, Texture, Parameter.bSrgb);
            if (!BoundUploader->TryGetTexture(Texture, Binding.Texture, Parameter.bSrgb))
            {
                if (BoundUploader->GetTextureState(Texture, Parameter.bSrgb) == AssetGpuState::Failed)
                {
                    ReportDiagnostic("Material texture GPU upload failed: " + Parameter.Identifier);
                }
                return Previous;
            }
            Binding.Lifetime = BoundUploader->GetTextureLifetime(Texture, Parameter.bSrgb);
        }
        Snapshot->Textures.push_back(Binding);
    }
    Snapshot->Revision = Material->Definition.Revision;
    for (const auto& Binding : Snapshot->Textures)
    {
        Snapshot->Revision = (Snapshot->Revision ^ Binding.Texture.Value) * 1099511628211ull;
    }
    if (!Dynamic && Compilation->State.load() == MaterialCompilationState::Ready)
    {
        Cached.Published = Snapshot;
        Cached.Diagnostic.clear();
    }
    if (DynamicCache && Compilation->State.load() == MaterialCompilationState::Ready)
    {
        DynamicCache->Published = Snapshot;
        DynamicCache->Revision = Dynamic->GetRevision();
        Cached.Diagnostic.clear();
    }
    return Snapshot;
}

SceneAssetResolver::FileRevision SceneAssetResolver::ReadFileRevision(const std::string& Path)
{
    FileRevision Revision;
    std::error_code Error;
    Revision.bExists = std::filesystem::is_regular_file(Path, Error);
    if (Revision.bExists)
    {
        Revision.Modified = std::filesystem::last_write_time(Path, Error);
        Revision.Size = std::filesystem::file_size(Path, Error);
    }
    return Revision;
}

void SceneAssetResolver::PollMaterialChanges()
{
    AssertGameThread();
    const auto Now = std::chrono::steady_clock::now();
    if (Now - LastPoll < std::chrono::milliseconds(100))
    {
        return;
    }
    LastPoll = Now;
    for (auto Iterator = TextureFiles.begin(); Iterator != TextureFiles.end();)
    {
        AssetRegistryEntry Entry;
        if (!BoundRegistry->TryResolveKey(Iterator->second.Asset, Entry) || Entry.AbsolutePath != Iterator->first)
        {
            Iterator = TextureFiles.erase(Iterator);
            continue;
        }
        auto& Pair = *Iterator;
        ++Iterator;
        auto& Texture = Pair.second;
        const auto Revision = ReadFileRevision(Pair.first);
        if (!(Revision == Texture.Revision))
        {
            Texture.Revision = Revision;
            Texture.Changed = Now;
            Texture.bPendingChange = true;
        }
        if (Texture.Prepared && Texture.PreparationJob.IsCompleted())
        {
            const auto Prepared = Texture.Prepared;
            Texture.Prepared.reset();
            if (Prepared->Revision == Texture.Revision)
            {
                if (!Prepared->Diagnostic.HasError())
                {
                    const auto Diagnostic = BoundRegistry->UpdateSourceFingerprint(Texture.Asset.Asset, Prepared->Hash);
                    if (!Diagnostic.HasError())
                    {
                        BoundManager->InvalidateAsset(Texture.Asset.Asset);
                        BoundUploader->InvalidateAsset(Texture.Asset.Asset);
                    }
                    else
                    {
                        PrintString("Material texture update failed: " + Diagnostic.Message);
                    }
                }
                else
                {
                    PrintString("Material texture update failed: " + Prepared->Diagnostic.Message);
                }
                Texture.bPendingChange = false;
            }
        }
        if (Texture.bPendingChange && !Texture.Prepared && Now - Texture.Changed >= std::chrono::milliseconds(200))
        {
            auto Prepared = std::make_shared<TextureFingerprintPreparation>();
            Prepared->Revision = Texture.Revision;
            Texture.Prepared = Prepared;
            Texture.PreparationJob = JobSystem::Get()->Schedule([Prepared, Path = Pair.first]()
            {
                ContentHash::TryHashFile(Path, Prepared->Hash, Prepared->Diagnostic);
            });
        }
    }
    for (auto Iterator = DynamicMaterials.begin(); Iterator != DynamicMaterials.end();)
    {
        if (Iterator->second.Instance.expired())
        {
            Iterator = DynamicMaterials.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
    for (auto& Pair : Materials)
    {
        auto& Cached = Pair.second;
        for (auto& File : Cached.Files)
        {
            const auto Revision = ReadFileRevision(File.first);
            if (!(Revision == File.second))
            {
                File.second = Revision;
                Cached.Changed = Now;
                Cached.bPendingChange = true;
            }
        }
        if (Cached.bPendingChange && Now - Cached.Changed >= std::chrono::milliseconds(200))
        {
            BoundManager->InvalidateAsset(Pair.first.Asset);
            Cached.bPendingChange = false;
        }
    }
}

void SceneAssetResolver::Clear()
{
    AssertGameThread();
    Materials.clear();
    DynamicMaterials.clear();
    TextureFiles.clear();
}

std::string SceneAssetResolver::GetMaterialDiagnostic(const AssetKey& Key) const
{
    const auto Found = Materials.find(Key);
    if (Found != Materials.end())
    {
        return Found->second.Diagnostic;
    }
    return {};
}
