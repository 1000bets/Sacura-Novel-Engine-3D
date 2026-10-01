#include "Prefab/PrefabUtility.h"

#include "Assets/AssetRegistry.h"
#include "Assets/ContentHash.h"
#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"
#include "Prefab/PrefabGraphOps.h"
#include "Prefab/PrefabLibrary.h"
#include "Prefab/PrefabSceneBuilder.h"
#include "Reflection/Json/ReflectionJson.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <unordered_set>

namespace
{
struct SceneDeleter
{
    void operator()(Scene* Target) const
    {
        if (Target == nullptr)
        {
            return;
        }
        if (MemorySubsystem* Memory = MemorySubsystem::Get())
        {
            Memory->DestroyObject(Target);
        }
        else
        {
            delete Target;
        }
    }
};
using ScopedScene = std::unique_ptr<Scene, SceneDeleter>;

bool IsInsideDirectory(const std::filesystem::path& Candidate, const std::filesystem::path& Directory)
{
    // Compare canonical, case-insensitive path strings so that Windows temp paths
    // (drive-letter case, 8.3 short names, mixed separators) still resolve as "inside".
    std::error_code CandidateError;
    std::error_code DirectoryError;
    std::filesystem::path NormalizedCandidate = std::filesystem::weakly_canonical(Candidate, CandidateError);
    std::filesystem::path NormalizedDirectory = std::filesystem::weakly_canonical(Directory, DirectoryError);
    if (CandidateError)
    {
        NormalizedCandidate = Candidate.lexically_normal();
    }
    if (DirectoryError)
    {
        NormalizedDirectory = Directory.lexically_normal();
    }

    std::filesystem::path Relative = NormalizedCandidate.lexically_relative(NormalizedDirectory);
    if (Relative.empty() || (!Relative.native().empty() && Relative.native()[0] == '.'))
    {
        // Fall back to a case-insensitive prefix test (weakly_canonical does not
        // normalize case on Windows, so "C:\Users" vs "c:\users" can slip through).
        auto ToComparable = [](const std::filesystem::path& Value)
        {
            std::string Text = Value.lexically_normal().generic_string();
            while (!Text.empty() && Text.back() == '/')
            {
                Text.pop_back();
            }
#ifdef _WIN32
            for (char& Character : Text)
            {
                Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
            }
#endif
            return Text;
        };
        const std::string CandidateText = ToComparable(NormalizedCandidate);
        const std::string DirectoryText = ToComparable(NormalizedDirectory);
        return CandidateText.size() > DirectoryText.size()
            && CandidateText.compare(0, DirectoryText.size(), DirectoryText) == 0
            && CandidateText[DirectoryText.size()] == '/';
    }
    return true;
}

std::vector<GameObject*> CollectAddedObjectRoots(Scene& SourceScene, const std::string& InstanceId)
{
    std::vector<GameObject*> Added;
    for (GameObject* Candidate : SourceScene.GetAllObjects())
    {
        if (Candidate != nullptr
            && Candidate->GetPrefabInstanceId() != InstanceId
            && Candidate->GetParent() != nullptr
            && Candidate->GetParent()->GetPrefabInstanceId() == InstanceId)
        {
            Added.push_back(Candidate);
        }
    }
    return Added;
}
}

// ============================================================================ queries

bool PrefabUtility::IsPartOfPrefabInstance(const GameObject* Object)
{
    return Object != nullptr && (Object->IsOwnedByPrefabInstance() || Object->GetPrefabInstance() != nullptr);
}

bool PrefabUtility::IsOutermostInstanceRoot(const GameObject* Object)
{
    return Object != nullptr && Object->GetPrefabInstance() != nullptr;
}

bool PrefabUtility::IsAddedObject(const GameObject* Object)
{
    return Object != nullptr
        && !IsPartOfPrefabInstance(Object)
        && Object->GetParent() != nullptr
        && IsPartOfPrefabInstance(Object->GetParent());
}

GameObject* PrefabUtility::GetOutermostInstanceRoot(const GameObject* Object)
{
    if (Object == nullptr)
    {
        return nullptr;
    }
    if (Object->GetPrefabInstance() != nullptr)
    {
        return const_cast<GameObject*>(Object);
    }
    if (!Object->IsOwnedByPrefabInstance() || Object->GetScene() == nullptr)
    {
        return nullptr;
    }
    return PrefabSceneBuilder::FindInstanceRoot(*Object->GetScene(), Object->GetPrefabInstanceId());
}

bool PrefabUtility::IsNestedPrefabRoot(const GameObject* Object, AssetId* OutNestedPrefab)
{
    GameObject* Root = GetOutermostInstanceRoot(Object);
    if (Root == nullptr || Root == Object || Root->GetPrefabInstance()->Base == nullptr)
    {
        return false;
    }
    const std::shared_ptr<PrefabInstanceData>& Data = Root->GetPrefabInstance();
    const std::string SourceId = PrefabIds::Combine(Data->InstanceId, Object->GetPersistentId());
    for (const PrefabGraphInstance& Nested : Data->Base->Instances)
    {
        if (Nested.RootObjectId == SourceId)
        {
            if (OutNestedPrefab != nullptr)
            {
                *OutNestedPrefab = Nested.Prefab;
            }
            return true;
        }
    }
    return false;
}

AssetId PrefabUtility::GetPrefabAsset(const GameObject* InstanceRoot)
{
    GameObject* Root = GetOutermostInstanceRoot(InstanceRoot);
    return Root != nullptr ? Root->GetPrefabInstance()->Prefab : AssetId{};
}

// ============================================================================ instantiation

GameObject* PrefabUtility::InstantiatePrefab(
    Scene& TargetScene,
    const AssetId& Prefab,
    GameObject* Parent,
    std::string& OutError)
{
    AssertGameThread();
    if (!Prefab.IsValid())
    {
        OutError = "Cannot instantiate an invalid prefab asset id";
        return nullptr;
    }
    nlohmann::json Record = nlohmann::json::object();
    Record["kind"] = PrefabInstanceEntryKind;
    Record["id"] = PrefabIds::Generate();
    Record["prefab"] = Prefab.ToString();
    Record["parent"] = nullptr;
    Record["modifications"] = nlohmann::json::object();

    PrefabGraphOps::ExpandOptions Options;
    Options.bAllowMissingPrefabs = false;
    std::vector<AssetId> Stack;
    PrefabGraph Graph;
    if (!PrefabGraphOps::Expand(nlohmann::json::array({Record}), Options, Stack, Graph, OutError))
    {
        return nullptr;
    }
    std::vector<GameObject*> Roots;
    if (!PrefabSceneBuilder::Materialize(TargetScene, Graph, Parent, Roots, OutError) || Roots.empty())
    {
        return nullptr;
    }
    return Roots.front();
}

// ============================================================================ overrides

bool PrefabUtility::GetOverrides(
    Scene& SourceScene,
    GameObject& InstanceRoot,
    std::vector<PrefabOverride>& OutOverrides,
    std::string& OutError)
{
    OutOverrides.clear();
    const std::shared_ptr<PrefabInstanceData> Data = InstanceRoot.GetPrefabInstance();
    if (Data == nullptr)
    {
        OutError = "Object is not a prefab instance root";
        return false;
    }
    if (Data->IsMissing())
    {
        OutError = "Prefab asset is missing";
        return false;
    }
    nlohmann::json Modifications;
    if (!PrefabSceneBuilder::ComputeLiveModifications(SourceScene, InstanceRoot, Modifications, OutError))
    {
        return false;
    }
    OutOverrides = PrefabGraphOps::DescribeModifications(*Data->Base, Data->InstanceId, Modifications);
    for (GameObject* Added : CollectAddedObjectRoots(SourceScene, Data->InstanceId))
    {
        PrefabOverride Override;
        Override.Kind = PrefabOverrideKind::AddedObject;
        Override.HostObjectId = Added->GetPersistentId();
        Override.SourceObjectId = PrefabIds::Combine(Data->InstanceId, Added->GetParent()->GetPersistentId());
        Override.DisplayText = "+ " + Added->GetName();
        OutOverrides.push_back(std::move(Override));
    }
    return true;
}

void PrefabUtility::CollectOverrideMarkers(
    Scene& SourceScene,
    GameObject& InstanceRoot,
    std::unordered_set<std::string>& OutMarkers)
{
    std::vector<PrefabOverride> Overrides;
    std::string Error;
    if (!GetOverrides(SourceScene, InstanceRoot, Overrides, Error))
    {
        return;
    }
    for (const PrefabOverride& Override : Overrides)
    {
        switch (Override.Kind)
        {
        case PrefabOverrideKind::ObjectProperty:
            OutMarkers.insert(Override.HostObjectId + "/" + Override.Property);
            break;
        case PrefabOverrideKind::ComponentProperty:
            OutMarkers.insert(Override.HostComponentId + "/" + Override.Property);
            break;
        case PrefabOverrideKind::AddedComponent:
            OutMarkers.insert(Override.HostComponentId + "/*");
            break;
        case PrefabOverrideKind::AddedObject:
            OutMarkers.insert(Override.HostObjectId + "/*");
            break;
        case PrefabOverrideKind::RemovedComponent:
            OutMarkers.insert(Override.HostObjectId + "/-component");
            break;
        case PrefabOverrideKind::ReparentedObject:
            OutMarkers.insert(Override.HostObjectId + "/parent");
            break;
        case PrefabOverrideKind::RemovedObject:
            OutMarkers.insert(Override.HostObjectId + "/removed");
            break;
        }
    }
}

bool PrefabUtility::RevertOverrides(
    Scene& TargetScene,
    GameObject& InstanceRoot,
    const std::vector<PrefabOverride>& Overrides,
    std::string& OutError)
{
    AssertGameThread();
    const std::shared_ptr<PrefabInstanceData> Data = InstanceRoot.GetPrefabInstance();
    if (Data == nullptr || Data->IsMissing())
    {
        OutError = "Object is not a resolvable prefab instance root";
        return false;
    }
    nlohmann::json Modifications;
    if (!PrefabSceneBuilder::ComputeLiveModifications(TargetScene, InstanceRoot, Modifications, OutError))
    {
        return false;
    }
    for (const PrefabOverride& Override : Overrides)
    {
        if (Override.Kind == PrefabOverrideKind::AddedObject)
        {
            if (GameObject* Added = TargetScene.FindByPersistentId(Override.HostObjectId))
            {
                TargetScene.DestroyGameObject(Added);
            }
            continue;
        }
        PrefabGraphOps::EraseOverride(Modifications, Override);
    }
    return PrefabSceneBuilder::ReconcileInstance(TargetScene, InstanceRoot, Data->Base, &Modifications, OutError);
}

bool PrefabUtility::RevertAll(Scene& TargetScene, GameObject& InstanceRoot, std::string& OutError)
{
    AssertGameThread();
    const std::shared_ptr<PrefabInstanceData> Data = InstanceRoot.GetPrefabInstance();
    if (Data == nullptr || Data->IsMissing())
    {
        OutError = "Object is not a resolvable prefab instance root";
        return false;
    }
    for (GameObject* Added : CollectAddedObjectRoots(TargetScene, Data->InstanceId))
    {
        TargetScene.DestroyGameObject(Added);
    }
    const nlohmann::json Empty = nlohmann::json::object();
    return PrefabSceneBuilder::ReconcileInstance(TargetScene, InstanceRoot, Data->Base, &Empty, OutError);
}

bool PrefabUtility::ApplyOverrides(
    Scene& TargetScene,
    GameObject& InstanceRoot,
    const std::vector<PrefabOverride>& Selection,
    std::string& OutError)
{
    AssertGameThread();
    const std::shared_ptr<PrefabInstanceData> Data = InstanceRoot.GetPrefabInstance();
    if (Data == nullptr || Data->IsMissing())
    {
        OutError = "Object is not a resolvable prefab instance root";
        return false;
    }
    const std::string InstanceId = Data->InstanceId;
    const AssetId Prefab = Data->Prefab;

    std::vector<PrefabOverride> Overrides = Selection;
    if (Overrides.empty() && !GetOverrides(TargetScene, InstanceRoot, Overrides, OutError))
    {
        return false;
    }
    if (Overrides.empty())
    {
        return true;
    }
    std::stable_sort(Overrides.begin(), Overrides.end(), [](const PrefabOverride& Left, const PrefabOverride& Right)
    {
        auto Rank = [](PrefabOverrideKind Kind)
        {
            return Kind == PrefabOverrideKind::AddedObject ? 0 : (Kind == PrefabOverrideKind::ReparentedObject ? 2 : 1);
        };
        return Rank(Left.Kind) < Rank(Right.Kind);
    });

    ScopedScene Temp(MemorySubsystem::Get()->NewObject<Scene>("PrefabApplyScene"));
    GameObject* TempRoot = nullptr;
    if (!LoadPrefabIntoScene(Prefab, *Temp, TempRoot, OutError))
    {
        return false;
    }
    auto Map = [&InstanceId](const std::string& Id)
    {
        return PrefabIds::Combine(InstanceId, Id);
    };

    std::string Skipped;
    for (const PrefabOverride& Override : Overrides)
    {
        switch (Override.Kind)
        {
        case PrefabOverrideKind::ObjectProperty:
        {
            GameObject* Live = TargetScene.FindByPersistentId(Override.HostObjectId);
            GameObject* Target = Temp->FindByPersistentId(Override.SourceObjectId);
            PrefabGraphObject Captured;
            std::string Error;
            if (Live == nullptr || Target == nullptr || !PrefabSceneBuilder::CaptureObject(*Live, Captured, Error))
            {
                Skipped += Override.DisplayText + "; ";
                break;
            }
            PrefabSceneBuilder::ApplyObjectField(*Target, Override.Property, PrefabGraphOps::ReadObjectField(Captured, Override.Property));
            break;
        }
        case PrefabOverrideKind::ComponentProperty:
        {
            Component* Live = PrefabSceneBuilder::FindComponent(TargetScene, Override.HostComponentId);
            Component* Target = PrefabSceneBuilder::FindComponent(*Temp, Override.SourceComponentId);
            if (Live == nullptr || Target == nullptr)
            {
                Skipped += Override.DisplayText + "; ";
                break;
            }
            if (Override.Property == "bEnabled")
            {
                Target->SetEnabled(Live->IsEnabled());
                break;
            }
            ReflectionJsonDocument Current;
            if (!ReflectionJson::SerializeObject(Live, Current).bOk || !Current.Properties.contains(Override.Property))
            {
                Skipped += Override.DisplayText + "; ";
                break;
            }
            ReflectionJsonDocument Patch;
            Patch.Type = Current.Type;
            Patch.TypeVersion = Current.TypeVersion;
            Patch.Properties[Override.Property] = Current.Properties.at(Override.Property);
            ReflectionJson::DeserializeInto(Target, Patch, true);
            break;
        }
        case PrefabOverrideKind::AddedComponent:
        {
            Component* Live = PrefabSceneBuilder::FindComponent(TargetScene, Override.HostComponentId);
            GameObject* Target = Temp->FindByPersistentId(Override.SourceObjectId);
            PrefabGraphComponent Captured;
            Component* Created = nullptr;
            std::string Error;
            if (Live == nullptr || Target == nullptr
                || !PrefabSceneBuilder::CaptureComponent(*Live, Captured, Error))
            {
                Skipped += Override.DisplayText + "; ";
                break;
            }
            // XOR with the instance id: after propagation the component keeps its current id.
            Captured.Id = Map(Captured.Id);
            Captured.InstanceId.clear();
            if (!PrefabSceneBuilder::CreateComponent(Captured, Created, Error) || Target->AddExistingComponent(Created, true) == nullptr)
            {
                Skipped += Override.DisplayText + "; ";
            }
            break;
        }
        case PrefabOverrideKind::RemovedComponent:
            if (Component* Target = PrefabSceneBuilder::FindComponent(*Temp, Override.SourceComponentId))
            {
                Target->GetGameObject()->RemoveComponent(Target);
            }
            break;
        case PrefabOverrideKind::RemovedObject:
            if (GameObject* Target = Temp->FindByPersistentId(Override.SourceObjectId))
            {
                if (Target != TempRoot)
                {
                    Temp->DestroyGameObject(Target);
                }
            }
            break;
        case PrefabOverrideKind::AddedObject:
        {
            GameObject* Live = TargetScene.FindByPersistentId(Override.HostObjectId);
            GameObject* TargetParent = Temp->FindByPersistentId(Override.SourceObjectId);
            if (Live == nullptr || TargetParent == nullptr)
            {
                Skipped += Override.DisplayText + "; ";
                break;
            }
            std::vector<GameObject*> Subtree;
            PrefabSceneBuilder::CollectSubtreeExcludingInstance(Live, InstanceId, Subtree);
            nlohmann::json Entries;
            bool bHasInstances = false;
            std::string Error;
            PrefabGraph Graph;
            std::vector<AssetId> Stack;
            std::vector<GameObject*> Roots;
            if (!PrefabSceneBuilder::WriteEntries(TargetScene, Subtree, true, Entries, bHasInstances, Error)
                || !PrefabSceneBuilder::RemapEntryIds(Entries, Map, Error)
                || !PrefabGraphOps::Expand(Entries, PrefabGraphOps::ExpandOptions{}, Stack, Graph, Error)
                || !PrefabSceneBuilder::Materialize(*Temp, Graph, TargetParent, Roots, Error))
            {
                Skipped += Override.DisplayText + " (" + Error + "); ";
            }
            break;
        }
        case PrefabOverrideKind::ReparentedObject:
        {
            GameObject* Live = TargetScene.FindByPersistentId(Override.HostObjectId);
            GameObject* Target = Temp->FindByPersistentId(Override.SourceObjectId);
            GameObject* NewParent = (Live != nullptr && Live->GetParent() != nullptr)
                ? Temp->FindByPersistentId(Map(Live->GetParent()->GetPersistentId()))
                : nullptr;
            if (Target == nullptr || NewParent == nullptr || Target == TempRoot || !Target->SetParent(NewParent))
            {
                Skipped += Override.DisplayText + "; ";
            }
            break;
        }
        }
    }

    if (!SavePrefabFromScene(*Temp, Prefab, OutError))
    {
        return false;
    }
    Temp.reset();

    std::string RefreshErrors;
    RefreshInstances(TargetScene, Prefab, RefreshErrors);
    if (!Skipped.empty())
    {
        PrintString("Prefab: some overrides were not applied: " + Skipped);
    }
    return true;
}

// ============================================================================ structure

bool PrefabUtility::Unpack(Scene& TargetScene, GameObject& InstanceRoot, PrefabUnpackMode Mode, std::string& OutError)
{
    AssertGameThread();
    const std::shared_ptr<PrefabInstanceData> Data = InstanceRoot.GetPrefabInstance();
    if (Data == nullptr)
    {
        OutError = "Object is not a prefab instance root";
        return false;
    }
    const std::string InstanceId = Data->InstanceId;

    if (Mode == PrefabUnpackMode::Completely)
    {
        std::vector<GameObject*> Subtree;
        PrefabSceneBuilder::CollectSubtree(&InstanceRoot, Subtree);
        std::unordered_set<std::string> Instances = {InstanceId};
        for (GameObject* Object : Subtree)
        {
            if (Object->GetPrefabInstance() != nullptr)
            {
                Instances.insert(Object->GetPrefabInstance()->InstanceId);
            }
        }
        for (GameObject* Object : TargetScene.GetAllObjects())
        {
            if (Instances.count(Object->GetPrefabInstanceId()) != 0)
            {
                Object->SetPrefabInstanceId({});
                Object->SetPrefabInstance(nullptr);
            }
            for (Component* ComponentInstance : Object->GetAllComponents())
            {
                if (Instances.count(ComponentInstance->GetPrefabInstanceId()) != 0)
                {
                    ComponentInstance->SetPrefabInstanceId({});
                }
            }
        }
        InstanceRoot.SetPrefabInstance(nullptr);
        return true;
    }

    if (Data->IsMissing())
    {
        OutError = "Cannot unpack one level: prefab asset is missing";
        return false;
    }
    const std::shared_ptr<const PrefabGraph> Base = Data->Base;
    auto Map = [&InstanceId](const std::string& Id)
    {
        return PrefabIds::Combine(InstanceId, Id);
    };

    for (GameObject* Object : TargetScene.GetAllObjects())
    {
        if (Object->GetPrefabInstanceId() != InstanceId)
        {
            continue;
        }
        const PrefabGraphObject* Source = Base->FindObject(Map(Object->GetPersistentId()));
        Object->SetPrefabInstanceId(Source != nullptr && !Source->InstanceId.empty() ? Map(Source->InstanceId) : std::string{});
        for (Component* ComponentInstance : Object->GetAllComponents())
        {
            if (ComponentInstance->GetPrefabInstanceId() != InstanceId)
            {
                continue;
            }
            std::string NewOwner;
            if (Source != nullptr)
            {
                const std::string SourceComponentId = Map(ComponentInstance->GetPersistentId());
                for (const PrefabGraphComponent& SourceComponent : Source->Components)
                {
                    if (SourceComponent.Id == SourceComponentId && !SourceComponent.InstanceId.empty())
                    {
                        NewOwner = Map(SourceComponent.InstanceId);
                        break;
                    }
                }
            }
            ComponentInstance->SetPrefabInstanceId(NewOwner);
        }
    }
    InstanceRoot.SetPrefabInstance(nullptr);

    for (const PrefabGraphInstance& Nested : Base->Instances)
    {
        GameObject* NestedRoot = TargetScene.FindByPersistentId(Map(Nested.RootObjectId));
        if (NestedRoot == nullptr)
        {
            continue;
        }
        auto NestedData = std::make_shared<PrefabInstanceData>();
        NestedData->InstanceId = Map(Nested.InstanceId);
        NestedData->Prefab = Nested.Prefab;
        NestedData->Base = Nested.Base;
        NestedData->Modifications = Nested.Modifications;
        if (Nested.Base == nullptr)
        {
            NestedData->MissingRecord = Nested.MissingRecord;
            NestedData->MissingRecord["id"] = NestedData->InstanceId;
        }
        NestedRoot->SetPrefabInstance(NestedData);
        std::string Ignored;
        nlohmann::json Normalized;
        if (NestedData->Base != nullptr
            && PrefabSceneBuilder::ComputeLiveModifications(TargetScene, *NestedRoot, Normalized, Ignored))
        {
            NestedData->Modifications = std::move(Normalized);
        }
    }
    return true;
}

bool PrefabUtility::ValidateNewPrefabPath(const std::filesystem::path& AbsolutePath, std::string& OutError)
{
    AssetRegistry* Registry = PrefabLibrary::Get().GetRegistry();
    if (Registry == nullptr || Registry->GetGameContentRoot().empty())
    {
        OutError = "No project content is open";
        return false;
    }
    if (AbsolutePath.extension() != PrefabFileExtension)
    {
        OutError = "Prefab file must use the .prefab extension";
        return false;
    }
    if (!IsInsideDirectory(AbsolutePath, Registry->GetGameContentRoot()))
    {
        OutError = "Prefab must be saved inside the project's Content folder";
        return false;
    }
    std::error_code Error;
    if (std::filesystem::exists(AbsolutePath, Error))
    {
        OutError = "File already exists: " + AbsolutePath.generic_string();
        return false;
    }
    if (Registry->LeafNameExists(AbsolutePath.stem().string()))
    {
        OutError = "Asset name '" + AbsolutePath.stem().string() + "' is already used in the project";
        return false;
    }
    return true;
}

bool PrefabUtility::RegisterPrefabFile(const std::filesystem::path& AbsolutePath, AssetId& OutPrefab, std::string& OutError)
{
    AssetRegistry* Registry = PrefabLibrary::Get().GetRegistry();
    if (Registry == nullptr)
    {
        OutError = "Asset registry is not bound";
        return false;
    }
    const std::string Relative = AbsolutePath.lexically_normal()
        .lexically_relative(Registry->GetGameContentRoot().lexically_normal())
        .generic_string();
    AssetMetadata Metadata;
    Metadata.Guid = Guid::Generate();
    Metadata.Type = PrefabAssetType;
    AssetDiagnostic HashError;
    ContentHash::TryHashFile(AbsolutePath.string(), Metadata.SourceFingerprint, HashError);
    const AssetDiagnostic Registered = Registry->RegisterExistingAsset("/Game/" + Relative, Metadata);
    if (Registered.HasError())
    {
        OutError = Registered.Message;
        return false;
    }
    OutPrefab = Metadata.Guid;
    return true;
}

bool PrefabUtility::CreatePrefabAsset(
    Scene& TargetScene,
    GameObject& Root,
    const std::filesystem::path& AbsolutePath,
    bool bAsVariant,
    AssetId& OutPrefab,
    std::string& OutError)
{
    AssertGameThread();
    if (!ValidateNewPrefabPath(AbsolutePath, OutError))
    {
        return false;
    }
    if (Root.IsOwnedByPrefabInstance() && Root.GetPrefabInstance() == nullptr)
    {
        OutError = "Object is part of a prefab instance; unpack the instance first";
        return false;
    }
    std::vector<GameObject*> Subtree;
    PrefabSceneBuilder::CollectSubtree(&Root, Subtree);
    const std::unordered_set<GameObject*> InSubtree(Subtree.begin(), Subtree.end());
    for (GameObject* Object : Subtree)
    {
        if (!Object->IsOwnedByPrefabInstance())
        {
            continue;
        }
        GameObject* OwnerRoot = PrefabSceneBuilder::FindInstanceRoot(TargetScene, Object->GetPrefabInstanceId());
        if (OwnerRoot == nullptr || InSubtree.count(OwnerRoot) == 0)
        {
            OutError = "Subtree contains objects of a prefab instance whose root is outside the subtree";
            return false;
        }
    }

    if (Root.GetPrefabInstance() != nullptr && !bAsVariant)
    {
        if (!Unpack(TargetScene, Root, PrefabUnpackMode::OutermostRoot, OutError))
        {
            return false;
        }
    }

    nlohmann::json Entries;
    bool bHasInstances = false;
    if (!PrefabSceneBuilder::WriteEntries(TargetScene, Subtree, true, Entries, bHasInstances, OutError))
    {
        return false;
    }
    // id_in_prefab = Instance ^ id_live  =>  after connecting, Instance ^ id_in_prefab == id_live.
    const std::string InstanceId = PrefabIds::Generate();
    if (!PrefabSceneBuilder::RemapEntryIds(Entries, [&InstanceId](const std::string& Id)
        {
            return PrefabIds::Combine(InstanceId, Id);
        }, OutError))
    {
        return false;
    }

    const nlohmann::json Document = PrefabLibrary::MakeDocument(AbsolutePath.stem().string(), std::move(Entries));
    if (!PrefabLibrary::WriteDocumentFile(AbsolutePath, Document, OutError)
        || !RegisterPrefabFile(AbsolutePath, OutPrefab, OutError))
    {
        return false;
    }
    std::shared_ptr<const PrefabGraph> Base = PrefabLibrary::Get().Resolve(OutPrefab, OutError);
    if (Base == nullptr)
    {
        return false;
    }

    // Connect the live subtree as an instance of the new prefab (ids are preserved; legacy
    // non-GUID ids become their canonical GUID form).
    for (GameObject* Object : Subtree)
    {
        const std::string Canonical = PrefabIds::Canonicalize(Object->GetPersistentId());
        if (Canonical != Object->GetPersistentId())
        {
            PrefabSceneBuilder::SetPersistentId(*Object, Canonical);
        }
        Object->SetPrefabInstanceId(InstanceId);
        Object->SetPrefabInstance(nullptr);
        for (Component* ComponentInstance : Object->GetAllComponents())
        {
            const std::string CanonicalComponent = PrefabIds::Canonicalize(ComponentInstance->GetPersistentId());
            if (CanonicalComponent != ComponentInstance->GetPersistentId())
            {
                PrefabSceneBuilder::SetPersistentId(*ComponentInstance, CanonicalComponent);
            }
            ComponentInstance->SetPrefabInstanceId(InstanceId);
        }
    }
    auto Data = std::make_shared<PrefabInstanceData>();
    Data->InstanceId = InstanceId;
    Data->Prefab = OutPrefab;
    Data->Base = Base;
    Root.SetPrefabInstance(Data);
    return PrefabSceneBuilder::ReconcileInstance(TargetScene, Root, Base, nullptr, OutError);
}

bool PrefabUtility::CreateVariantAsset(
    const AssetId& BasePrefab,
    const std::filesystem::path& AbsolutePath,
    AssetId& OutPrefab,
    std::string& OutError)
{
    AssertGameThread();
    if (!ValidateNewPrefabPath(AbsolutePath, OutError))
    {
        return false;
    }
    if (PrefabLibrary::Get().Resolve(BasePrefab, OutError) == nullptr)
    {
        return false;
    }
    nlohmann::json Record = nlohmann::json::object();
    Record["kind"] = PrefabInstanceEntryKind;
    Record["id"] = PrefabIds::Generate();
    Record["name"] = AbsolutePath.stem().string();
    Record["prefab"] = BasePrefab.ToString();
    Record["parent"] = nullptr;
    Record["modifications"] = nlohmann::json::object();
    const nlohmann::json Document = PrefabLibrary::MakeDocument(AbsolutePath.stem().string(), nlohmann::json::array({Record}));
    return PrefabLibrary::WriteDocumentFile(AbsolutePath, Document, OutError)
        && RegisterPrefabFile(AbsolutePath, OutPrefab, OutError);
}

// ============================================================================ documents / propagation

int PrefabUtility::RefreshInstances(Scene& TargetScene, const AssetId& ChangedPrefab, std::string& OutErrors)
{
    AssertGameThread();
    std::vector<ObjectHandle> Roots;
    for (GameObject* Candidate : TargetScene.GetAllObjects())
    {
        if (Candidate == nullptr || Candidate->GetPrefabInstance() == nullptr)
        {
            continue;
        }
        const std::shared_ptr<PrefabInstanceData>& Data = Candidate->GetPrefabInstance();
        const bool bAffected = !ChangedPrefab.IsValid()
            || Data->Prefab == ChangedPrefab
            || Data->IsMissing()
            || Data->Base->DependsOn(ChangedPrefab);
        if (bAffected)
        {
            Roots.push_back(Candidate->GetObjectHandle());
        }
    }

    int Updated = 0;
    for (const ObjectHandle& Handle : Roots)
    {
        GameObject* Root = TargetScene.FindByHandle(Handle);
        if (Root == nullptr || Root->GetPrefabInstance() == nullptr)
        {
            continue;
        }
        std::shared_ptr<PrefabInstanceData> Data = Root->GetPrefabInstance();
        std::string Error;
        std::shared_ptr<const PrefabGraph> NewBase = PrefabLibrary::Get().Resolve(Data->Prefab, Error);
        if (NewBase == nullptr)
        {
            OutErrors += Error + "\n";
            continue;
        }
        if (NewBase == Data->Base)
        {
            continue;
        }
        const nlohmann::json* Forced = Data->IsMissing() ? &Data->Modifications : nullptr;
        const nlohmann::json Saved = Data->Modifications;
        if (!PrefabSceneBuilder::ReconcileInstance(TargetScene, *Root, NewBase, Forced != nullptr ? &Saved : nullptr, Error))
        {
            OutErrors += Error + "\n";
            continue;
        }
        ++Updated;
    }
    return Updated;
}

bool PrefabUtility::LoadPrefabIntoScene(const AssetId& Prefab, Scene& TargetScene, GameObject*& OutRoot, std::string& OutError)
{
    OutRoot = nullptr;
    nlohmann::json Document;
    if (!PrefabLibrary::Get().ReadDocument(Prefab, Document, OutError))
    {
        return false;
    }
    PrefabGraphOps::ExpandOptions Options;
    Options.bCanonicalizeIds = true;
    Options.bAllowMissingPrefabs = true;
    std::vector<AssetId> Stack = {Prefab};
    PrefabGraph Graph;
    if (!PrefabGraphOps::Expand(Document.at("objects"), Options, Stack, Graph, OutError))
    {
        return false;
    }
    std::vector<GameObject*> Roots;
    if (!PrefabSceneBuilder::Materialize(TargetScene, Graph, nullptr, Roots, OutError))
    {
        return false;
    }
    if (Roots.size() != 1)
    {
        OutError = "Prefab must contain exactly one root object";
        return false;
    }
    OutRoot = Roots.front();
    TargetScene.SetName(Document.contains("name") && Document.at("name").is_string()
        ? Document.at("name").get<std::string>()
        : PrefabLibrary::Get().GetPrefabDisplayName(Prefab));
    return true;
}

bool PrefabUtility::BuildPrefabDocument(const Scene& SourceScene, const std::string& Name, nlohmann::json& OutDocument, std::string& OutError)
{
    const std::vector<GameObject*> Roots = SourceScene.GetRootObjects();
    if (Roots.size() != 1)
    {
        OutError = "Prefab must contain exactly one root object (found " + std::to_string(Roots.size()) + ")";
        return false;
    }
    std::vector<GameObject*> Subtree;
    PrefabSceneBuilder::CollectSubtree(Roots.front(), Subtree);
    nlohmann::json Entries;
    bool bHasInstances = false;
    if (!PrefabSceneBuilder::WriteEntries(SourceScene, Subtree, true, Entries, bHasInstances, OutError))
    {
        return false;
    }
    OutDocument = PrefabLibrary::MakeDocument(Name, std::move(Entries));
    return true;
}

bool PrefabUtility::SavePrefabFromScene(const Scene& SourceScene, const AssetId& Prefab, std::string& OutError)
{
    AssertGameThread();
    std::filesystem::path AbsolutePath;
    if (!PrefabLibrary::Get().TryGetPrefabPath(Prefab, AbsolutePath))
    {
        OutError = "Prefab asset is not registered";
        return false;
    }
    for (GameObject* Candidate : SourceScene.GetAllObjects())
    {
        if (Candidate->GetPrefabInstance() != nullptr
            && PrefabLibrary::Get().DependsOn(Candidate->GetPrefabInstance()->Prefab, Prefab))
        {
            OutError = "Prefab cannot contain an instance of itself (nesting cycle via '" + Candidate->GetName() + "')";
            return false;
        }
    }
    nlohmann::json Document;
    if (!BuildPrefabDocument(SourceScene, AbsolutePath.stem().string(), Document, OutError)
        || !PrefabLibrary::WriteDocumentFile(AbsolutePath, Document, OutError))
    {
        return false;
    }
    PrefabLibrary::Get().NotifyPrefabChanged(Prefab);
    return true;
}

bool PrefabUtility::SavePrefabFromSceneAs(
    const Scene& SourceScene,
    const std::filesystem::path& AbsolutePath,
    AssetId& OutPrefab,
    std::string& OutError)
{
    AssertGameThread();
    if (!ValidateNewPrefabPath(AbsolutePath, OutError))
    {
        return false;
    }
    nlohmann::json Document;
    return BuildPrefabDocument(SourceScene, AbsolutePath.stem().string(), Document, OutError)
        && PrefabLibrary::WriteDocumentFile(AbsolutePath, Document, OutError)
        && RegisterPrefabFile(AbsolutePath, OutPrefab, OutError);
}
