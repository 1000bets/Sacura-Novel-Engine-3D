#include "Prefab/PrefabSceneBuilder.h"

#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Game/SceneJson.h"
#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"
#include "Prefab/PrefabGraphOps.h"
#include "Reflection/Class.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/ReflectionSubsystem.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace
{
std::unordered_map<std::string, GameObject*> BuildInstanceRootMap(const Scene& SourceScene)
{
    std::unordered_map<std::string, GameObject*> Roots;
    for (GameObject* Candidate : SourceScene.GetAllObjects())
    {
        if (Candidate != nullptr && Candidate->GetPrefabInstance() != nullptr)
        {
            Roots[Candidate->GetPrefabInstance()->InstanceId] = Candidate;
        }
    }
    return Roots;
}

void DestroyDetachedComponent(Component* ComponentInstance)
{
    if (ComponentInstance == nullptr)
    {
        return;
    }
    if (MemorySubsystem* Memory = MemorySubsystem::Get())
    {
        Memory->DestroyObject(ComponentInstance);
    }
    else
    {
        delete ComponentInstance;
    }
}
}

void PrefabSceneBuilder::SetPersistentId(Object& Target, const std::string& PersistentId)
{
    Target.PersistentId = PersistentId;
}

void PrefabSceneBuilder::CollectSubtree(GameObject* Root, std::vector<GameObject*>& OutObjects)
{
    if (Root == nullptr)
    {
        return;
    }
    OutObjects.push_back(Root);
    for (GameObject* Child : Root->GetChildren())
    {
        CollectSubtree(Child, OutObjects);
    }
}

void PrefabSceneBuilder::CollectSubtreeExcludingInstance(
    GameObject* Root,
    const std::string& StopAtInstanceId,
    std::vector<GameObject*>& OutObjects)
{
    if (Root == nullptr || (!StopAtInstanceId.empty() && Root->GetPrefabInstanceId() == StopAtInstanceId))
    {
        return;
    }
    OutObjects.push_back(Root);
    for (GameObject* Child : Root->GetChildren())
    {
        CollectSubtreeExcludingInstance(Child, StopAtInstanceId, OutObjects);
    }
}

bool PrefabSceneBuilder::CaptureComponent(const Component& Source, PrefabGraphComponent& OutComponent, std::string& OutError)
{
    if (Source.GetClass() == nullptr)
    {
        OutError = "Component '" + Source.GetName() + "' has no Class (not reflected)";
        return false;
    }
    ReflectionJsonDocument Document;
    const ReflectionDiagnostic Serialized = ReflectionJson::SerializeObject(&Source, Document);
    if (!Serialized.bOk)
    {
        OutError = Serialized.Message;
        return false;
    }
    OutComponent = PrefabGraphComponent{};
    OutComponent.Id = Source.GetPersistentId();
    OutComponent.Type = Document.Type.Value;
    OutComponent.TypeVersion = Document.TypeVersion;
    OutComponent.bEnabled = Source.IsEnabled();
    OutComponent.Properties = std::move(Document.Properties);
    OutComponent.InstanceId = Source.GetPrefabInstanceId();
    return true;
}

bool PrefabSceneBuilder::CaptureObject(const GameObject& Source, PrefabGraphObject& OutObject, std::string& OutError)
{
    OutObject = PrefabGraphObject{};
    OutObject.Id = Source.GetPersistentId();
    OutObject.Name = Source.GetName();
    OutObject.bActive = Source.IsActive();
    OutObject.bVisual = Source.IsVisual();
    OutObject.LocalTransform = Source.GetTransform();
    OutObject.ParentId = Source.GetParent() != nullptr ? Source.GetParent()->GetPersistentId() : std::string{};
    OutObject.InstanceId = Source.GetPrefabInstanceId();
    for (Component* ComponentInstance : Source.GetAllComponents())
    {
        if (ComponentInstance == nullptr)
        {
            OutError = "GameObject contains a null Component";
            return false;
        }
        PrefabGraphComponent Captured;
        if (!CaptureComponent(*ComponentInstance, Captured, OutError))
        {
            return false;
        }
        OutObject.Components.push_back(std::move(Captured));
    }
    return true;
}

bool PrefabSceneBuilder::CaptureInstance(
    const Scene& SourceScene,
    const std::string& InstanceId,
    std::vector<PrefabGraphObject>& OutObjects,
    std::string& OutError)
{
    OutObjects.clear();
    for (GameObject* Candidate : SourceScene.GetAllObjects())
    {
        if (Candidate == nullptr)
        {
            continue;
        }

        // The live root stores the authoritative instance record in PrefabInstanceData.
        // In case the lightweight ownership marker got out of sync, do not drop the
        // root from the captured instance: otherwise WriteEntries serializes the root
        // as a plain object and the prefab link is permanently lost on scene reload.
        const std::shared_ptr<PrefabInstanceData>& RootData = Candidate->GetPrefabInstance();
        const bool bIsMatchingRoot = RootData != nullptr && RootData->InstanceId == InstanceId;
        if (Candidate->GetPrefabInstanceId() != InstanceId && !bIsMatchingRoot)
        {
            continue;
        }
        PrefabGraphObject Captured;
        if (!CaptureObject(*Candidate, Captured, OutError))
        {
            return false;
        }
        OutObjects.push_back(std::move(Captured));
    }
    return true;
}

GameObject* PrefabSceneBuilder::FindInstanceRoot(const Scene& SourceScene, const std::string& InstanceId)
{
    if (InstanceId.empty())
    {
        return nullptr;
    }
    for (GameObject* Candidate : SourceScene.GetAllObjects())
    {
        if (Candidate != nullptr
            && Candidate->GetPrefabInstance() != nullptr
            && Candidate->GetPrefabInstance()->InstanceId == InstanceId)
        {
            return Candidate;
        }
    }
    return nullptr;
}

Component* PrefabSceneBuilder::FindComponent(const Scene& SourceScene, const std::string& ComponentPersistentId)
{
    for (GameObject* Candidate : SourceScene.GetAllObjects())
    {
        if (Candidate == nullptr)
        {
            continue;
        }
        for (Component* ComponentInstance : Candidate->GetAllComponents())
        {
            if (ComponentInstance != nullptr && ComponentInstance->GetPersistentId() == ComponentPersistentId)
            {
                return ComponentInstance;
            }
        }
    }
    return nullptr;
}

bool PrefabSceneBuilder::CreateComponent(const PrefabGraphComponent& Data, Component*& OutComponent, std::string& OutError)
{
    OutComponent = nullptr;
    ReflectionSubsystem& Reflection = ReflectionSubsystem::Get();
    if (!Reflection.IsInitialized())
    {
        OutError = "ReflectionSubsystem is not initialized";
        return false;
    }
    Object* Created = Reflection.CreateInstance(TypeId{Data.Type});
    Component* ComponentInstance = dynamic_cast<Component*>(Created);
    if (ComponentInstance == nullptr)
    {
        if (Created != nullptr)
        {
            if (MemorySubsystem* Memory = MemorySubsystem::Get())
            {
                Memory->DestroyObject(Created);
            }
        }
        OutError = "Unknown or unsupported component type '" + Data.Type + "'";
        return false;
    }

    SetPersistentId(*ComponentInstance, Data.Id);
    ReflectionJsonDocument Document;
    Document.Type = TypeId{Data.Type};
    Document.TypeVersion = Data.TypeVersion;
    Document.Properties = Data.Properties.is_object() ? Data.Properties : nlohmann::json::object();
    const ReflectionDiagnostic Applied = ReflectionJson::DeserializeInto(ComponentInstance, Document, true);
    if (!Applied.bOk)
    {
        DestroyDetachedComponent(ComponentInstance);
        OutError = Applied.Message;
        return false;
    }
    ComponentInstance->SetEnabled(Data.bEnabled);
    ComponentInstance->SetPrefabInstanceId(Data.InstanceId);
    OutComponent = ComponentInstance;
    return true;
}

void PrefabSceneBuilder::ApplyObjectField(GameObject& Target, const std::string& Field, const nlohmann::json& Value)
{
    PrefabGraphObject Data;
    Data.Name = Target.GetName();
    Data.bActive = Target.IsActive();
    Data.bVisual = Target.IsVisual();
    Data.LocalTransform = Target.GetTransform();
    PrefabGraphOps::WriteObjectField(Data, Field, Value);
    Target.SetName(Data.Name);
    Target.SetActive(Data.bActive);
    Target.SetVisual(Data.bVisual);
    Target.SetTransform(Data.LocalTransform);
}

bool PrefabSceneBuilder::Materialize(
    Scene& TargetScene,
    const PrefabGraph& Graph,
    GameObject* OptionalParent,
    std::vector<GameObject*>& OutRoots,
    std::string& OutError)
{
    OutRoots.clear();
    if (MemorySubsystem::Get() == nullptr || !ReflectionSubsystem::Get().IsInitialized())
    {
        OutError = "Memory or Reflection is not ready";
        return false;
    }
    if (OptionalParent != nullptr && OptionalParent->GetScene() != &TargetScene)
    {
        OutError = "Optional parent does not belong to target Scene";
        return false;
    }

    std::unordered_set<std::string> ExistingObjects;
    std::unordered_set<std::string> ExistingComponents;
    for (GameObject* Existing : TargetScene.GetAllObjects())
    {
        ExistingObjects.insert(Existing->GetPersistentId());
        for (Component* ExistingComponent : Existing->GetAllComponents())
        {
            ExistingComponents.insert(ExistingComponent->GetPersistentId());
        }
    }
    for (const PrefabGraphObject& Object : Graph.Objects)
    {
        if (ExistingObjects.count(Object.Id) != 0)
        {
            OutError = "Duplicate or empty subtree object id '" + Object.Id + "'";
            return false;
        }
        for (const PrefabGraphComponent& ComponentData : Object.Components)
        {
            if (ExistingComponents.count(ComponentData.Id) != 0)
            {
                OutError = "Duplicate or empty component id in subtree '" + ComponentData.Id + "'";
                return false;
            }
        }
    }

    std::vector<ObjectHandle> CreatedHandles;
    std::unordered_map<std::string, GameObject*> ById;
    std::vector<Component*> DeferredCreate;
    auto Fail = [&](const std::string& Message)
    {
        for (auto Iterator = CreatedHandles.rbegin(); Iterator != CreatedHandles.rend(); ++Iterator)
        {
            if (GameObject* Created = TargetScene.FindByHandle(*Iterator))
            {
                TargetScene.DestroyGameObject(Created);
            }
        }
        OutRoots.clear();
        OutError = Message;
        return false;
    };

    for (const PrefabGraphObject& Object : Graph.Objects)
    {
        GameObject* Created = TargetScene.CreateGameObject(Object.Name);
        if (Created == nullptr)
        {
            return Fail("Failed to create GameObject");
        }
        CreatedHandles.push_back(Created->GetObjectHandle());
        SetPersistentId(*Created, Object.Id);
        Created->SetActive(Object.bActive);
        Created->SetVisual(Object.bVisual);
        Created->SetTransform(Object.LocalTransform);
        Created->SetPrefabInstanceId(Object.InstanceId);
        ById[Object.Id] = Created;

        for (const PrefabGraphComponent& ComponentData : Object.Components)
        {
            Component* ComponentInstance = nullptr;
            std::string ComponentError;
            if (!CreateComponent(ComponentData, ComponentInstance, ComponentError))
            {
                return Fail(ComponentError);
            }
            if (Created->AddExistingComponent(ComponentInstance, false) == nullptr)
            {
                DestroyDetachedComponent(ComponentInstance);
                return Fail("Failed to attach component '" + ComponentData.Id + "'");
            }
            DeferredCreate.push_back(ComponentInstance);
        }
    }

    for (const PrefabGraphObject& Object : Graph.Objects)
    {
        GameObject* Created = ById[Object.Id];
        if (Object.ParentId.empty())
        {
            if (OptionalParent != nullptr && !Created->SetParent(OptionalParent))
            {
                return Fail("Failed to attach restored subtree to parent");
            }
            OutRoots.push_back(Created);
            continue;
        }
        const auto Parent = ById.find(Object.ParentId);
        if (Parent == ById.end())
        {
            return Fail("Missing parent reference '" + Object.ParentId + "' for object '" + Object.Id + "'");
        }
        if (!Created->SetParent(Parent->second))
        {
            return Fail("Failed to apply parent '" + Object.ParentId + "' to '" + Object.Id + "'");
        }
    }

    for (const PrefabGraphInstance& Instance : Graph.Instances)
    {
        const auto Root = ById.find(Instance.RootObjectId);
        if (Root == ById.end())
        {
            continue;
        }
        auto Data = std::make_shared<PrefabInstanceData>();
        Data->InstanceId = Instance.InstanceId;
        Data->Prefab = Instance.Prefab;
        Data->Base = Instance.Base;
        Data->Modifications = Instance.Modifications;
        Data->MissingRecord = Instance.MissingRecord;
        Data->MissingChildParentSources = Instance.MissingChildParentSources;
        Root->second->SetPrefabInstance(std::move(Data));
    }

    for (Component* ComponentInstance : DeferredCreate)
    {
        ComponentInstance->OnCreate();
    }
    return true;
}

bool PrefabSceneBuilder::WriteEntries(
    const Scene& SourceScene,
    const std::vector<GameObject*>& Objects,
    bool bSubtree,
    nlohmann::json& OutEntries,
    bool& bOutHasInstances,
    std::string& OutError)
{
    OutEntries = nlohmann::json::array();
    bOutHasInstances = false;
    const std::unordered_set<const GameObject*> InSet(Objects.begin(), Objects.end());
    const std::unordered_map<std::string, GameObject*> Roots = BuildInstanceRootMap(SourceScene);

    auto RecordRootInSet = [&](const std::string& InstanceId) -> GameObject*
    {
        const auto Found = Roots.find(InstanceId);
        if (Found == Roots.end() || InSet.count(Found->second) == 0)
        {
            return nullptr;
        }
        return Found->second;
    };

    // Returns {parent, parentSource}.
    auto ParentReference = [&](const GameObject& Child) -> std::pair<std::string, std::string>
    {
        GameObject* Parent = Child.GetParent();
        if (Parent == nullptr || (bSubtree && InSet.count(Parent) == 0))
        {
            return {};
        }
        const std::string& OwnerInstance = Parent->GetPrefabInstanceId();
        GameObject* OwnerRoot = OwnerInstance.empty() ? nullptr : RecordRootInSet(OwnerInstance);
        if (OwnerRoot == nullptr)
        {
            return {Parent->GetPersistentId(), {}};
        }
        const std::shared_ptr<PrefabInstanceData>& Data = OwnerRoot->GetPrefabInstance();
        if (Data->IsMissing())
        {
            const auto Saved = Data->MissingChildParentSources.find(Child.GetPersistentId());
            if (Saved != Data->MissingChildParentSources.end())
            {
                return {OwnerInstance, Saved->second};
            }
            return {Parent->GetPersistentId(), {}};
        }
        return {OwnerInstance, PrefabIds::Combine(OwnerInstance, Parent->GetPersistentId())};
    };

    for (GameObject* ObjectInstance : Objects)
    {
        if (ObjectInstance == nullptr)
        {
            OutError = "Scene contains a null GameObject";
            return false;
        }
        // A prefab root has two pieces of instance metadata: the lightweight owner id
        // inherited from Object and the full PrefabInstanceData record.  The latter is
        // authoritative for roots.  Using only GetPrefabInstanceId() here makes a
        // transiently desynchronized root get written as a plain object, which strips
        // all prefab information from the scene document.
        std::string OwnerInstance = ObjectInstance->GetPrefabInstanceId();
        if (const std::shared_ptr<PrefabInstanceData>& RootData = ObjectInstance->GetPrefabInstance())
        {
            OwnerInstance = RootData->InstanceId;
        }
        GameObject* OwnerRoot = OwnerInstance.empty() ? nullptr : RecordRootInSet(OwnerInstance);
        if (OwnerRoot != nullptr)
        {
            if (OwnerRoot != ObjectInstance)
            {
                continue;
            }
            bOutHasInstances = true;
            const std::shared_ptr<PrefabInstanceData>& Data = ObjectInstance->GetPrefabInstance();
            const std::pair<std::string, std::string> Parent = ParentReference(*ObjectInstance);
            nlohmann::json Record = nlohmann::json::object();
            if (Data->IsMissing() && Data->MissingRecord.is_object())
            {
                Record = Data->MissingRecord;
                Record.erase("parentSource");
            }
            else
            {
                std::vector<PrefabGraphObject> Live;
                if (!CaptureInstance(SourceScene, OwnerInstance, Live, OutError))
                {
                    return false;
                }
                Record["kind"] = PrefabInstanceEntryKind;
                Record["id"] = OwnerInstance;
                Record["name"] = ObjectInstance->GetName();
                Record["prefab"] = Data->Prefab.ToString();
                Record["modifications"] = PrefabGraphOps::ComputeModifications(*Data->Base, OwnerInstance, Live);
            }
            Record["parent"] = Parent.first.empty() ? nlohmann::json(nullptr) : nlohmann::json(Parent.first);
            if (!Parent.second.empty())
            {
                Record["parentSource"] = Parent.second;
            }
            OutEntries.push_back(std::move(Record));
            continue;
        }

        PrefabGraphObject Captured;
        if (!CaptureObject(*ObjectInstance, Captured, OutError))
        {
            return false;
        }
        const std::pair<std::string, std::string> Parent = ParentReference(*ObjectInstance);
        nlohmann::json Entry = PrefabGraphOps::WritePlainEntry(Captured, Parent.first, Parent.second);
        if (bSubtree && !OwnerInstance.empty())
        {
            // Part of an instance whose root is not in the subtree (delete/undo of an instance child).
            Entry["prefabOwner"] = OwnerInstance;
            for (size_t Index = 0; Index < Captured.Components.size(); ++Index)
            {
                if (!Captured.Components[Index].InstanceId.empty())
                {
                    Entry["components"][Index]["prefabOwner"] = Captured.Components[Index].InstanceId;
                }
            }
        }
        OutEntries.push_back(std::move(Entry));
    }
    return true;
}

bool PrefabSceneBuilder::RemapEntryIds(
    nlohmann::json& Entries,
    const std::function<std::string(const std::string&)>& Mapper,
    std::string& OutError)
{
    if (!Entries.is_array())
    {
        OutError = "Subtree JSON requires objects array";
        return false;
    }
    std::unordered_map<std::string, std::string> Table;
    auto AddId = [&](const nlohmann::json& Value, const char* What) -> bool
    {
        if (!Value.is_string() || Value.get<std::string>().empty())
        {
            OutError = std::string("Subtree ") + What + " requires string id";
            return false;
        }
        const std::string Id = Value.get<std::string>();
        if (!Table.emplace(Id, Mapper(Id)).second)
        {
            OutError = std::string("Subtree contains duplicate or empty ") + What + " id";
            return false;
        }
        return true;
    };

    for (const nlohmann::json& Entry : Entries)
    {
        if (!Entry.is_object() || !Entry.contains("id") || !AddId(Entry.at("id"), "object"))
        {
            if (OutError.empty())
            {
                OutError = "Subtree object requires string id";
            }
            return false;
        }
        if (Entry.contains("components") && Entry.at("components").is_array())
        {
            for (const nlohmann::json& ComponentEntry : Entry.at("components"))
            {
                if (!ComponentEntry.is_object() || !ComponentEntry.contains("id") || !AddId(ComponentEntry.at("id"), "component"))
                {
                    if (OutError.empty())
                    {
                        OutError = "Subtree component requires string id";
                    }
                    return false;
                }
            }
        }
        if (Entry.contains("modifications")
            && Entry.at("modifications").is_object()
            && Entry.at("modifications").contains("addedComponents")
            && Entry.at("modifications").at("addedComponents").is_object())
        {
            for (const auto& Pair : Entry.at("modifications").at("addedComponents").items())
            {
                if (!Pair.value().is_array())
                {
                    continue;
                }
                for (const nlohmann::json& ComponentEntry : Pair.value())
                {
                    if (ComponentEntry.is_object() && ComponentEntry.contains("id") && !AddId(ComponentEntry.at("id"), "component"))
                    {
                        return false;
                    }
                }
            }
        }
    }

    static const std::unordered_set<std::string> SourceSpaceKeys = {"parentSource", "source", "prefab", "type", "kind"};
    std::function<void(nlohmann::json&)> Replace = [&](nlohmann::json& Value)
    {
        if (Value.is_string())
        {
            const auto Found = Table.find(Value.get<std::string>());
            if (Found != Table.end())
            {
                Value = Found->second;
            }
            return;
        }
        if (Value.is_array())
        {
            for (nlohmann::json& Element : Value)
            {
                Replace(Element);
            }
            return;
        }
        if (Value.is_object())
        {
            Value.erase("prefabOwner");
            for (auto Iterator = Value.begin(); Iterator != Value.end(); ++Iterator)
            {
                if (SourceSpaceKeys.count(Iterator.key()) == 0)
                {
                    Replace(Iterator.value());
                }
            }
        }
    };
    Replace(Entries);
    return true;
}

bool PrefabSceneBuilder::ComputeLiveModifications(
    const Scene& SourceScene,
    const GameObject& InstanceRoot,
    nlohmann::json& OutModifications,
    std::string& OutError)
{
    const std::shared_ptr<PrefabInstanceData>& Data = InstanceRoot.GetPrefabInstance();
    if (Data == nullptr)
    {
        OutError = "Object is not a prefab instance root";
        return false;
    }
    if (Data->IsMissing())
    {
        OutModifications = Data->Modifications;
        return true;
    }
    std::vector<PrefabGraphObject> Live;
    if (!CaptureInstance(SourceScene, Data->InstanceId, Live, OutError))
    {
        return false;
    }
    OutModifications = PrefabGraphOps::ComputeModifications(*Data->Base, Data->InstanceId, Live);
    return true;
}

bool PrefabSceneBuilder::ReconcileInstance(
    Scene& TargetScene,
    GameObject& InstanceRoot,
    std::shared_ptr<const PrefabGraph> NewBase,
    const nlohmann::json* ForcedModifications,
    std::string& OutError)
{
    std::shared_ptr<PrefabInstanceData> Data = InstanceRoot.GetPrefabInstance();
    if (Data == nullptr)
    {
        OutError = "Object is not a prefab instance root";
        return false;
    }
    if (NewBase == nullptr)
    {
        OutError = "Prefab base graph is not available";
        return false;
    }

    nlohmann::json Modifications = nlohmann::json::object();
    if (ForcedModifications != nullptr)
    {
        Modifications = *ForcedModifications;
    }
    else if (!ComputeLiveModifications(TargetScene, InstanceRoot, Modifications, OutError))
    {
        return false;
    }

    const std::string InstanceId = Data->InstanceId;
    std::vector<PrefabGraphObject> Desired;
    std::string RootId;
    PrefabGraphOps::Instantiate(*NewBase, InstanceId, Modifications, Desired, RootId);

    if (InstanceRoot.GetPersistentId() != RootId)
    {
        GameObject* Conflict = TargetScene.FindByPersistentId(RootId);
        if (Conflict != nullptr && Conflict != &InstanceRoot)
        {
            OutError = "Prefab root id conflicts with an existing object";
            return false;
        }
        SetPersistentId(InstanceRoot, RootId);
    }

    const std::unordered_map<std::string, std::string> MissingChildren = Data->IsMissing()
        ? Data->MissingChildParentSources
        : std::unordered_map<std::string, std::string>{};

    if (!SyncLiveObjects(TargetScene, InstanceId, Desired, InstanceRoot, InstanceRoot.GetParent(), OutError))
    {
        return false;
    }

    Data->Base = NewBase;
    Data->MissingRecord = nlohmann::json();
    Data->MissingChildParentSources.clear();
    for (const auto& Pair : MissingChildren)
    {
        GameObject* Child = TargetScene.FindByPersistentId(Pair.first);
        GameObject* Parent = TargetScene.FindByPersistentId(PrefabIds::Combine(InstanceId, Pair.second));
        if (Child != nullptr && Parent != nullptr)
        {
            Child->SetParent(Parent);
        }
    }

    std::string NormalizeError;
    nlohmann::json Normalized;
    if (ComputeLiveModifications(TargetScene, InstanceRoot, Normalized, NormalizeError))
    {
        Data->Modifications = std::move(Normalized);
    }
    return true;
}

bool PrefabSceneBuilder::SyncLiveObjects(
    Scene& TargetScene,
    const std::string& InstanceId,
    const std::vector<PrefabGraphObject>& Desired,
    GameObject& InstanceRoot,
    GameObject* RootParent,
    std::string& OutError)
{
    std::unordered_map<std::string, const PrefabGraphObject*> DesiredById;
    for (const PrefabGraphObject& Object : Desired)
    {
        DesiredById[Object.Id] = &Object;
    }
    const std::string RootId = InstanceRoot.GetPersistentId();

    auto CollectOwned = [&]()
    {
        std::unordered_map<std::string, GameObject*> Owned;
        for (GameObject* Candidate : TargetScene.GetAllObjects())
        {
            if (Candidate != nullptr && Candidate->GetPrefabInstanceId() == InstanceId)
            {
                Owned[Candidate->GetPersistentId()] = Candidate;
            }
        }
        return Owned;
    };
    std::unordered_map<std::string, GameObject*> Live = CollectOwned();
    Live[RootId] = &InstanceRoot;
    InstanceRoot.SetPrefabInstanceId(InstanceId);

    // 1. Create (or adopt) missing objects. Adoption happens after Apply: an added object that
    //    was written into the prefab keeps its id and simply becomes prefab-owned.
    for (const PrefabGraphObject& Object : Desired)
    {
        if (Live.count(Object.Id) != 0)
        {
            continue;
        }
        GameObject* Existing = TargetScene.FindByPersistentId(Object.Id);
        if (Existing != nullptr)
        {
            Existing->SetPrefabInstanceId(InstanceId);
            Existing->SetPrefabInstance(nullptr);
            Live[Object.Id] = Existing;
            continue;
        }
        GameObject* Created = TargetScene.CreateGameObject(Object.Name);
        if (Created == nullptr)
        {
            OutError = "Failed to create GameObject during prefab sync";
            return false;
        }
        SetPersistentId(*Created, Object.Id);
        Created->SetPrefabInstanceId(InstanceId);
        Live[Object.Id] = Created;
    }

    // 2. Destroy owned objects that no longer exist in the prefab. Children that must survive
    //    (other prefab objects, local additions, nested scene instances) are moved to the root first.
    std::vector<std::string> Extras;
    for (const auto& Pair : Live)
    {
        if (DesiredById.count(Pair.first) == 0)
        {
            Extras.push_back(Pair.first);
        }
    }
    for (const std::string& ExtraId : Extras)
    {
        GameObject* Extra = TargetScene.FindByPersistentId(ExtraId);
        if (Extra == nullptr || Extra == &InstanceRoot || Extra->GetPrefabInstanceId() != InstanceId)
        {
            continue;
        }
        const std::vector<GameObject*> Children = Extra->GetChildren();
        for (GameObject* Child : Children)
        {
            if (DesiredById.count(Child->GetPersistentId()) != 0 || Child->GetPrefabInstanceId() != InstanceId)
            {
                Child->SetParent(&InstanceRoot);
            }
        }
        TargetScene.DestroyGameObject(Extra);
    }
    Live = CollectOwned();

    // 3. Hierarchy: detach every object whose parent changes, then attach (never forms cycles).
    auto TargetParent = [&](const PrefabGraphObject& Object) -> GameObject*
    {
        if (Object.Id == RootId)
        {
            return RootParent;
        }
        if (Object.ParentId.empty())
        {
            return nullptr;
        }
        GameObject* Parent = TargetScene.FindByPersistentId(Object.ParentId);
        return Parent != nullptr ? Parent : &InstanceRoot;
    };
    for (const PrefabGraphObject& Object : Desired)
    {
        GameObject* LiveObject = Live[Object.Id];
        if (LiveObject != nullptr && LiveObject->GetParent() != TargetParent(Object))
        {
            LiveObject->SetParent(nullptr);
        }
    }
    for (const PrefabGraphObject& Object : Desired)
    {
        GameObject* LiveObject = Live[Object.Id];
        GameObject* Parent = TargetParent(Object);
        if (LiveObject != nullptr && LiveObject->GetParent() != Parent && !LiveObject->SetParent(Parent))
        {
            PrintString("Prefab: failed to restore parent of '" + Object.Id + "' during sync");
        }
    }

    // 4. Fields and components.
    for (const PrefabGraphObject& Object : Desired)
    {
        GameObject* LiveObject = Live[Object.Id];
        if (LiveObject == nullptr)
        {
            continue;
        }
        if (LiveObject->GetName() != Object.Name)
        {
            LiveObject->SetName(Object.Name);
        }
        LiveObject->SetActive(Object.bActive);
        LiveObject->SetVisual(Object.bVisual);
        const Transform& Current = LiveObject->GetTransform();
        if (!SceneJson::NearlyEqual(Current.Position, Object.LocalTransform.Position)
            || !SceneJson::NearlyEqual(Current.Rotation, Object.LocalTransform.Rotation)
            || !SceneJson::NearlyEqual(Current.Scale, Object.LocalTransform.Scale))
        {
            LiveObject->SetTransform(Object.LocalTransform);
        }
        SyncComponents(*LiveObject, Object);
    }
    return true;
}

void PrefabSceneBuilder::SyncComponents(GameObject& Target, const PrefabGraphObject& Desired)
{
    std::unordered_set<std::string> DesiredIds;
    for (const PrefabGraphComponent& ComponentData : Desired.Components)
    {
        DesiredIds.insert(ComponentData.Id);
    }
    const std::vector<Component*> Existing = Target.GetAllComponents();
    for (Component* ComponentInstance : Existing)
    {
        if (DesiredIds.count(ComponentInstance->GetPersistentId()) == 0)
        {
            Target.RemoveComponent(ComponentInstance);
        }
    }

    for (const PrefabGraphComponent& ComponentData : Desired.Components)
    {
        Component* LiveComponent = nullptr;
        for (Component* Candidate : Target.GetAllComponents())
        {
            if (Candidate->GetPersistentId() == ComponentData.Id)
            {
                LiveComponent = Candidate;
                break;
            }
        }
        if (LiveComponent != nullptr
            && (LiveComponent->GetClass() == nullptr || LiveComponent->GetClass()->GetTypeId().Value != ComponentData.Type))
        {
            Target.RemoveComponent(LiveComponent);
            LiveComponent = nullptr;
        }

        if (LiveComponent == nullptr)
        {
            Component* Created = nullptr;
            std::string Error;
            if (!CreateComponent(ComponentData, Created, Error))
            {
                PrintString("Prefab: failed to create component during sync: " + Error);
                continue;
            }
            if (Target.AddExistingComponent(Created, true) == nullptr)
            {
                DestroyDetachedComponent(Created);
            }
            continue;
        }

        // Write only properties that really changed: some setters reset GPU/asset state.
        ReflectionJsonDocument Current;
        if (ReflectionJson::SerializeObject(LiveComponent, Current).bOk)
        {
            nlohmann::json Changed = nlohmann::json::object();
            for (auto Property = ComponentData.Properties.begin(); Property != ComponentData.Properties.end(); ++Property)
            {
                const auto Found = Current.Properties.find(Property.key());
                if (Found == Current.Properties.end() || !SceneJson::NearlyEqual(*Found, Property.value()))
                {
                    Changed[Property.key()] = Property.value();
                }
            }
            if (!Changed.empty())
            {
                ReflectionJsonDocument Patch;
                Patch.Type = TypeId{ComponentData.Type};
                Patch.TypeVersion = ComponentData.TypeVersion;
                Patch.Properties = std::move(Changed);
                const ReflectionDiagnostic Applied = ReflectionJson::DeserializeInto(LiveComponent, Patch, true);
                if (!Applied.bOk)
                {
                    PrintString("Prefab: failed to sync component properties: " + Applied.Message);
                }
            }
        }
        if (LiveComponent->IsEnabled() != ComponentData.bEnabled)
        {
            LiveComponent->SetEnabled(ComponentData.bEnabled);
        }
        LiveComponent->SetPrefabInstanceId(ComponentData.InstanceId);
    }
}
