#include "Prefab/PrefabGraphOps.h"

#include "Core/Threading/ThreadContext.h"
#include "World/Serialization/SceneJson.h"
#include "Prefab/PrefabLibrary.h"

#include <algorithm>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace
{
bool TryReadOptionalString(const nlohmann::json& Entry, const char* Key, std::string& OutValue, std::string& OutError)
{
    OutValue.clear();
    if (!Entry.contains(Key) || Entry.at(Key).is_null())
    {
        return true;
    }
    if (!Entry.at(Key).is_string())
    {
        OutError = std::string(Key) + " must be a string or null";
        return false;
    }
    OutValue = Entry.at(Key).get<std::string>();
    return true;
}

void AddDependency(std::vector<AssetId>& Dependencies, const AssetId& Asset)
{
    if (std::find(Dependencies.begin(), Dependencies.end(), Asset) == Dependencies.end())
    {
        Dependencies.push_back(Asset);
    }
}

std::string ShortTypeName(const std::string& Type)
{
    const size_t Dot = Type.find_last_of('.');
    return Dot == std::string::npos ? Type : Type.substr(Dot + 1);
}

struct ComponentLocation
{
    size_t ObjectIndex = 0;
    size_t ComponentIndex = 0;
};
}

// ----------------------------------------------------------------------------
// Object fields
// ----------------------------------------------------------------------------

const std::vector<std::string>& PrefabGraphOps::ObjectFieldNames()
{
    static const std::vector<std::string> Names = {"name", "bActive", "bVisual", "position", "rotation", "scale"};
    return Names;
}

nlohmann::json PrefabGraphOps::ReadObjectField(const PrefabGraphObject& Object, const std::string& Field)
{
    if (Field == "name")
    {
        return Object.Name;
    }
    if (Field == "bActive")
    {
        return Object.bActive;
    }
    if (Field == "bVisual")
    {
        return Object.bVisual;
    }
    if (Field == "position")
    {
        return SceneJson::Vector3ToJson(Object.LocalTransform.Position);
    }
    if (Field == "rotation")
    {
        return SceneJson::QuaternionToJson(Object.LocalTransform.Rotation);
    }
    if (Field == "scale")
    {
        return SceneJson::Vector3ToJson(Object.LocalTransform.Scale);
    }
    return nullptr;
}

void PrefabGraphOps::WriteObjectField(PrefabGraphObject& Object, const std::string& Field, const nlohmann::json& Value)
{
    std::string Ignored;
    if (Field == "name" && Value.is_string())
    {
        Object.Name = Value.get<std::string>();
    }
    else if (Field == "bActive" && Value.is_boolean())
    {
        Object.bActive = Value.get<bool>();
    }
    else if (Field == "bVisual" && Value.is_boolean())
    {
        Object.bVisual = Value.get<bool>();
    }
    else if (Field == "position")
    {
        SceneJson::TryReadVector3(Value, Object.LocalTransform.Position, Ignored);
    }
    else if (Field == "rotation")
    {
        SceneJson::TryReadQuaternion(Value, Object.LocalTransform.Rotation, Ignored);
    }
    else if (Field == "scale")
    {
        SceneJson::TryReadVector3(Value, Object.LocalTransform.Scale, Ignored);
    }
}

// ----------------------------------------------------------------------------
// Entry (de)serialization
// ----------------------------------------------------------------------------

bool PrefabGraphOps::ParseComponent(const nlohmann::json& Entry, PrefabGraphComponent& OutComponent, std::string& OutError)
{
    if (!Entry.is_object())
    {
        OutError = "component entry must be an object";
        return false;
    }
    if (!Entry.contains("id") || !Entry.at("id").is_string() || Entry.at("id").get<std::string>().empty())
    {
        OutError = "component requires string id";
        return false;
    }
    if (!Entry.contains("type") || !Entry.at("type").is_string())
    {
        OutError = "component requires string type";
        return false;
    }
    OutComponent = PrefabGraphComponent{};
    OutComponent.Id = Entry.at("id").get<std::string>();
    OutComponent.Type = Entry.at("type").get<std::string>();
    if (Entry.contains("typeVersion") && Entry.at("typeVersion").is_number_unsigned())
    {
        OutComponent.TypeVersion = Entry.at("typeVersion").get<uint32_t>();
    }
    if (Entry.contains("bEnabled") && Entry.at("bEnabled").is_boolean())
    {
        OutComponent.bEnabled = Entry.at("bEnabled").get<bool>();
    }
    if (Entry.contains("properties") && Entry.at("properties").is_object())
    {
        OutComponent.Properties = Entry.at("properties");
    }
    if (Entry.contains("prefabOwner") && Entry.at("prefabOwner").is_string())
    {
        OutComponent.InstanceId = Entry.at("prefabOwner").get<std::string>();
    }
    return true;
}

nlohmann::json PrefabGraphOps::WriteComponent(const PrefabGraphComponent& Component)
{
    nlohmann::json Entry = nlohmann::json::object();
    Entry["id"] = Component.Id;
    Entry["type"] = Component.Type;
    Entry["typeVersion"] = Component.TypeVersion;
    Entry["bEnabled"] = Component.bEnabled;
    Entry["properties"] = Component.Properties;
    return Entry;
}

bool PrefabGraphOps::ParsePlainEntry(
    const nlohmann::json& Entry,
    PrefabGraphObject& OutObject,
    std::string& OutParentRef,
    std::string& OutParentSource,
    std::string& OutError)
{
    OutObject = PrefabGraphObject{};
    if (!Entry.is_object())
    {
        OutError = "Scene object entry must be an object";
        return false;
    }
    if (!Entry.contains("id") || !Entry.at("id").is_string())
    {
        OutError = "Scene object requires string id";
        return false;
    }
    OutObject.Id = Entry.at("id").get<std::string>();
    if (OutObject.Id.empty())
    {
        OutError = "Duplicate or empty object id ''";
        return false;
    }
    OutObject.Name = OutObject.Id;
    if (Entry.contains("name") && Entry.at("name").is_string())
    {
        OutObject.Name = Entry.at("name").get<std::string>();
    }
    if (Entry.contains("bActive") && Entry.at("bActive").is_boolean())
    {
        OutObject.bActive = Entry.at("bActive").get<bool>();
    }
    if (Entry.contains("bVisual") && Entry.at("bVisual").is_boolean())
    {
        OutObject.bVisual = Entry.at("bVisual").get<bool>();
    }
    if (Entry.contains("prefabOwner") && Entry.at("prefabOwner").is_string())
    {
        OutObject.InstanceId = Entry.at("prefabOwner").get<std::string>();
    }
    if (Entry.contains("transform")
        && !SceneJson::TryReadTransform(Entry.at("transform"), OutObject.LocalTransform, OutError))
    {
        return false;
    }
    if (!TryReadOptionalString(Entry, "parent", OutParentRef, OutError))
    {
        OutError = "parent must be a string id or null";
        return false;
    }
    if (!TryReadOptionalString(Entry, "parentSource", OutParentSource, OutError))
    {
        return false;
    }
    if (Entry.contains("components"))
    {
        if (!Entry.at("components").is_array())
        {
            OutError = "components must be an array";
            return false;
        }
        for (const nlohmann::json& ComponentEntry : Entry.at("components"))
        {
            PrefabGraphComponent Component;
            if (!ParseComponent(ComponentEntry, Component, OutError))
            {
                return false;
            }
            OutObject.Components.push_back(std::move(Component));
        }
    }
    return true;
}

nlohmann::json PrefabGraphOps::WritePlainEntry(
    const PrefabGraphObject& Object,
    const std::string& ParentRef,
    const std::string& ParentSource)
{
    nlohmann::json Entry = nlohmann::json::object();
    Entry["id"] = Object.Id;
    Entry["name"] = Object.Name;
    Entry["bActive"] = Object.bActive;
    Entry["bVisual"] = Object.bVisual;
    Entry["transform"] = SceneJson::TransformToJson(Object.LocalTransform);
    if (ParentRef.empty())
    {
        Entry["parent"] = nullptr;
    }
    else
    {
        Entry["parent"] = ParentRef;
        if (!ParentSource.empty())
        {
            Entry["parentSource"] = ParentSource;
        }
    }
    Entry["components"] = nlohmann::json::array();
    for (const PrefabGraphComponent& Component : Object.Components)
    {
        Entry["components"].push_back(WriteComponent(Component));
    }
    return Entry;
}

// ----------------------------------------------------------------------------
// Expansion
// ----------------------------------------------------------------------------

bool PrefabGraphOps::Expand(
    const nlohmann::json& Entries,
    const ExpandOptions& Options,
    std::vector<AssetId>& ResolveStack,
    PrefabGraph& OutGraph,
    std::string& OutError)
{
    OutGraph = PrefabGraph{};
    if (!Entries.is_array())
    {
        OutError = "objects must be an array";
        return false;
    }

    struct PendingParent
    {
        std::string ObjectId;
        std::string ParentRef;
        std::string ParentSource;
    };
    std::vector<PendingParent> Pending;

    auto Canon = [&Options](const std::string& Id)
    {
        return Options.bCanonicalizeIds && !Id.empty() ? PrefabIds::Canonicalize(Id) : Id;
    };

    for (const nlohmann::json& Entry : Entries)
    {
        if (!Entry.is_object())
        {
            OutError = "Scene object entry must be an object";
            return false;
        }

        const bool bInstanceEntry = Entry.contains("kind")
            && Entry.at("kind").is_string()
            && Entry.at("kind").get<std::string>() == PrefabInstanceEntryKind;
        if (!bInstanceEntry)
        {
            PrefabGraphObject Object;
            std::string ParentRef;
            std::string ParentSource;
            if (!ParsePlainEntry(Entry, Object, ParentRef, ParentSource, OutError))
            {
                return false;
            }
            Object.Id = Canon(Object.Id);
            for (PrefabGraphComponent& Component : Object.Components)
            {
                Component.Id = Canon(Component.Id);
            }
            Pending.push_back({Object.Id, Canon(ParentRef), Canon(ParentSource)});
            OutGraph.Objects.push_back(std::move(Object));
            continue;
        }

        if (!Entry.contains("id") || !Entry.at("id").is_string() || Entry.at("id").get<std::string>().empty())
        {
            OutError = "Prefab instance requires string id";
            return false;
        }
        if (!Entry.contains("prefab") || !Entry.at("prefab").is_string())
        {
            OutError = "Prefab instance requires prefab asset id";
            return false;
        }
        AssetId PrefabAsset{};
        if (!Guid::TryParse(Entry.at("prefab").get<std::string>(), PrefabAsset))
        {
            OutError = "Prefab instance has invalid prefab asset id";
            return false;
        }
        std::string ParentRef;
        std::string ParentSource;
        if (!TryReadOptionalString(Entry, "parent", ParentRef, OutError)
            || !TryReadOptionalString(Entry, "parentSource", ParentSource, OutError))
        {
            return false;
        }
        nlohmann::json Modifications = nlohmann::json::object();
        if (Entry.contains("modifications") && Entry.at("modifications").is_object())
        {
            Modifications = Entry.at("modifications");
        }

        const std::string InstanceId = Canon(Entry.at("id").get<std::string>());
        std::string ResolveError;
        std::shared_ptr<const PrefabGraph> Base = PrefabLibrary::Get().ResolveWithStack(PrefabAsset, ResolveStack, ResolveError);
        const bool bCycle = ResolveError.find("cycle") != std::string::npos;
        if (Base == nullptr && (bCycle || !Options.bAllowMissingPrefabs))
        {
            OutError = ResolveError;
            return false;
        }

        PrefabGraphInstance Instance;
        Instance.InstanceId = InstanceId;
        Instance.Prefab = PrefabAsset;
        Instance.Modifications = Modifications;
        Instance.Base = Base;
        AddDependency(OutGraph.Dependencies, PrefabAsset);

        if (Base == nullptr)
        {
            PrintString("Prefab: missing prefab " + PrefabAsset.ToString() + " (" + ResolveError + "), keeping placeholder");
            PrefabGraphObject Placeholder;
            Placeholder.Id = InstanceId;
            Placeholder.Name = Entry.contains("name") && Entry.at("name").is_string()
                ? Entry.at("name").get<std::string>() + " (Missing Prefab)"
                : std::string("Missing Prefab");
            Placeholder.InstanceId = InstanceId;
            Instance.RootObjectId = InstanceId;
            Instance.MissingRecord = Entry;
            Pending.push_back({Placeholder.Id, Canon(ParentRef), Canon(ParentSource)});
            OutGraph.Objects.push_back(std::move(Placeholder));
            OutGraph.Instances.push_back(std::move(Instance));
            continue;
        }

        for (const AssetId& Dependency : Base->Dependencies)
        {
            AddDependency(OutGraph.Dependencies, Dependency);
        }

        std::vector<PrefabGraphObject> InstanceObjects;
        std::string RootId;
        Instantiate(*Base, InstanceId, Modifications, InstanceObjects, RootId);
        Instance.RootObjectId = RootId;
        Pending.push_back({RootId, Canon(ParentRef), Canon(ParentSource)});
        for (PrefabGraphObject& Object : InstanceObjects)
        {
            OutGraph.Objects.push_back(std::move(Object));
        }
        OutGraph.Instances.push_back(std::move(Instance));
    }

    std::unordered_set<std::string> SeenObjects;
    std::unordered_set<std::string> SeenComponents;
    for (const PrefabGraphObject& Object : OutGraph.Objects)
    {
        if (Object.Id.empty() || !SeenObjects.insert(Object.Id).second)
        {
            OutError = "Duplicate or empty object id '" + Object.Id + "'";
            return false;
        }
        for (const PrefabGraphComponent& Component : Object.Components)
        {
            if (Component.Id.empty() || !SeenComponents.insert(Component.Id).second)
            {
                OutError = "Duplicate or empty component id '" + Component.Id + "'";
                return false;
            }
        }
    }
    OutGraph.RebuildIndex();

    for (const PendingParent& Link : Pending)
    {
        if (Link.ParentRef.empty())
        {
            continue;
        }
        std::string Target = Link.ParentSource.empty()
            ? Link.ParentRef
            : PrefabIds::Combine(Link.ParentRef, Link.ParentSource);
        if (OutGraph.FindObject(Target) == nullptr)
        {
            PrefabGraphInstance* OwnerInstance = nullptr;
            for (PrefabGraphInstance& Candidate : OutGraph.Instances)
            {
                if (Candidate.InstanceId == Link.ParentRef)
                {
                    OwnerInstance = &Candidate;
                    break;
                }
            }
            if (OwnerInstance == nullptr || Link.ParentSource.empty())
            {
                OutError = "Missing parent reference '" + Link.ParentRef + "' for object '" + Link.ObjectId + "'";
                return false;
            }
            if (OwnerInstance->Base == nullptr)
            {
                OwnerInstance->MissingChildParentSources[Link.ObjectId] = Link.ParentSource;
            }
            else
            {
                PrintString("Prefab: object '" + Link.ObjectId
                    + "' lost its parent inside the prefab instance, attaching it to the instance root");
            }
            Target = OwnerInstance->RootObjectId;
        }
        if (Target == Link.ObjectId)
        {
            OutError = "Object '" + Link.ObjectId + "' cannot be its own parent";
            return false;
        }
        OutGraph.FindObject(Link.ObjectId)->ParentId = Target;
    }

    // Reparent overrides may reference host objects that no longer exist.
    for (PrefabGraphObject& Object : OutGraph.Objects)
    {
        if (Object.ParentId.empty() || OutGraph.FindObject(Object.ParentId) != nullptr)
        {
            continue;
        }
        const PrefabGraphInstance* OwnerInstance = OutGraph.FindInstance(Object.InstanceId);
        if (OwnerInstance == nullptr)
        {
            OutError = "Missing parent reference '" + Object.ParentId + "' for object '" + Object.Id + "'";
            return false;
        }
        PrintString("Prefab: override parent of '" + Object.Id + "' is missing, using the instance root");
        Object.ParentId = Object.Id == OwnerInstance->RootObjectId ? std::string{} : OwnerInstance->RootObjectId;
    }

    for (const PrefabGraphObject& Object : OutGraph.Objects)
    {
        std::unordered_set<std::string> Visited;
        std::string Current = Object.ParentId;
        while (!Current.empty())
        {
            if (Current == Object.Id || !Visited.insert(Current).second)
            {
                OutError = "Parent cycle detected involving object '" + Object.Id + "'";
                return false;
            }
            const PrefabGraphObject* Parent = OutGraph.FindObject(Current);
            Current = Parent != nullptr ? Parent->ParentId : std::string{};
        }
    }

    size_t RootCount = 0;
    for (const PrefabGraphObject& Object : OutGraph.Objects)
    {
        if (Object.ParentId.empty())
        {
            ++RootCount;
            OutGraph.RootId = Object.Id;
        }
    }
    if (RootCount != 1)
    {
        OutGraph.RootId.clear();
    }
    return true;
}

// ----------------------------------------------------------------------------
// Instantiation (base + modifications -> host objects)
// ----------------------------------------------------------------------------

void PrefabGraphOps::Instantiate(
    const PrefabGraph& Base,
    const std::string& InstanceId,
    const nlohmann::json& Modifications,
    std::vector<PrefabGraphObject>& OutObjects,
    std::string& OutRootId)
{
    OutObjects.clear();
    OutObjects.reserve(Base.Objects.size());
    auto Map = [&InstanceId](const std::string& SourceId)
    {
        return PrefabIds::Combine(InstanceId, SourceId);
    };

    for (const PrefabGraphObject& Source : Base.Objects)
    {
        PrefabGraphObject Object = Source;
        Object.Id = Map(Source.Id);
        Object.ParentId = Source.ParentId.empty() ? std::string{} : Map(Source.ParentId);
        Object.InstanceId = InstanceId;
        for (PrefabGraphComponent& Component : Object.Components)
        {
            RemapObjectReferences(Component.Properties, Map);
            Component.Id = Map(Component.Id);
            Component.InstanceId = InstanceId;
        }
        OutObjects.push_back(std::move(Object));
    }
    OutRootId = Map(Base.RootId);

    std::unordered_map<std::string, size_t> IndexById;
    auto RebuildIndex = [&]()
    {
        IndexById.clear();
        for (size_t Index = 0; Index < OutObjects.size(); ++Index)
        {
            IndexById[OutObjects[Index].Id] = Index;
        }
    };
    auto Find = [&](const std::string& HostId) -> PrefabGraphObject*
    {
        const auto Found = IndexById.find(HostId);
        return Found == IndexById.end() ? nullptr : &OutObjects[Found->second];
    };
    RebuildIndex();

    if (!Modifications.is_object())
    {
        return;
    }

    if (Modifications.contains("objects") && Modifications.at("objects").is_object())
    {
        for (auto Iterator = Modifications.at("objects").begin(); Iterator != Modifications.at("objects").end(); ++Iterator)
        {
            PrefabGraphObject* Object = Find(Map(Iterator.key()));
            if (Object == nullptr || !Iterator.value().is_object())
            {
                continue;
            }
            for (auto Field = Iterator.value().begin(); Field != Iterator.value().end(); ++Field)
            {
                WriteObjectField(*Object, Field.key(), Field.value());
            }
        }
    }

    if (Modifications.contains("components") && Modifications.at("components").is_object())
    {
        std::unordered_map<std::string, ComponentLocation> ComponentIndex;
        for (size_t ObjectIndex = 0; ObjectIndex < OutObjects.size(); ++ObjectIndex)
        {
            for (size_t ComponentIndexValue = 0; ComponentIndexValue < OutObjects[ObjectIndex].Components.size(); ++ComponentIndexValue)
            {
                ComponentIndex[OutObjects[ObjectIndex].Components[ComponentIndexValue].Id] = {ObjectIndex, ComponentIndexValue};
            }
        }
        for (auto Iterator = Modifications.at("components").begin(); Iterator != Modifications.at("components").end(); ++Iterator)
        {
            const auto Found = ComponentIndex.find(Map(Iterator.key()));
            if (Found == ComponentIndex.end() || !Iterator.value().is_object())
            {
                continue;
            }
            PrefabGraphComponent& Component = OutObjects[Found->second.ObjectIndex].Components[Found->second.ComponentIndex];
            const nlohmann::json& Fields = Iterator.value();
            if (Fields.contains("bEnabled") && Fields.at("bEnabled").is_boolean())
            {
                Component.bEnabled = Fields.at("bEnabled").get<bool>();
            }
            if (Fields.contains("properties") && Fields.at("properties").is_object())
            {
                for (auto Property = Fields.at("properties").begin(); Property != Fields.at("properties").end(); ++Property)
                {
                    Component.Properties[Property.key()] = Property.value();
                }
            }
        }
    }

    if (Modifications.contains("parents") && Modifications.at("parents").is_object())
    {
        for (auto Iterator = Modifications.at("parents").begin(); Iterator != Modifications.at("parents").end(); ++Iterator)
        {
            PrefabGraphObject* Object = Find(Map(Iterator.key()));
            if (Object == nullptr || Object->Id == OutRootId || !Iterator.value().is_object())
            {
                continue;
            }
            const nlohmann::json& Reference = Iterator.value();
            if (Reference.contains("source") && Reference.at("source").is_string())
            {
                Object->ParentId = Map(Reference.at("source").get<std::string>());
            }
            else if (Reference.contains("object") && Reference.at("object").is_string())
            {
                Object->ParentId = Reference.at("object").get<std::string>();
            }
        }
    }

    if (Modifications.contains("removedComponents") && Modifications.at("removedComponents").is_array())
    {
        std::unordered_set<std::string> Removed;
        for (const nlohmann::json& Value : Modifications.at("removedComponents"))
        {
            if (Value.is_string())
            {
                Removed.insert(Map(Value.get<std::string>()));
            }
        }
        for (PrefabGraphObject& Object : OutObjects)
        {
            Object.Components.erase(
                std::remove_if(Object.Components.begin(), Object.Components.end(), [&Removed](const PrefabGraphComponent& Component)
                {
                    return Removed.count(Component.Id) != 0;
                }),
                Object.Components.end());
        }
    }

    if (Modifications.contains("removedObjects") && Modifications.at("removedObjects").is_array())
    {
        std::unordered_set<std::string> Removed;
        for (const nlohmann::json& Value : Modifications.at("removedObjects"))
        {
            if (Value.is_string())
            {
                const std::string HostId = Map(Value.get<std::string>());
                if (HostId != OutRootId)
                {
                    Removed.insert(HostId);
                }
            }
        }
        if (!Removed.empty())
        {
            std::unordered_map<std::string, bool> Memo;
            std::function<bool(const std::string&, size_t)> IsRemoved = [&](const std::string& HostId, size_t Depth) -> bool
            {
                if (Removed.count(HostId) != 0)
                {
                    return true;
                }
                const auto Cached = Memo.find(HostId);
                if (Cached != Memo.end())
                {
                    return Cached->second;
                }
                const PrefabGraphObject* Object = Find(HostId);
                bool bResult = false;
                if (Object != nullptr && !Object->ParentId.empty() && Depth < OutObjects.size())
                {
                    bResult = IsRemoved(Object->ParentId, Depth + 1);
                }
                Memo[HostId] = bResult;
                return bResult;
            };
            std::vector<std::string> ToErase;
            for (const PrefabGraphObject& Object : OutObjects)
            {
                if (Object.Id != OutRootId && IsRemoved(Object.Id, 0))
                {
                    ToErase.push_back(Object.Id);
                }
            }
            const std::unordered_set<std::string> EraseSet(ToErase.begin(), ToErase.end());
            OutObjects.erase(
                std::remove_if(OutObjects.begin(), OutObjects.end(), [&EraseSet](const PrefabGraphObject& Object)
                {
                    return EraseSet.count(Object.Id) != 0;
                }),
                OutObjects.end());
            RebuildIndex();
        }
    }

    if (Modifications.contains("addedComponents") && Modifications.at("addedComponents").is_object())
    {
        for (auto Iterator = Modifications.at("addedComponents").begin(); Iterator != Modifications.at("addedComponents").end(); ++Iterator)
        {
            PrefabGraphObject* Object = Find(Map(Iterator.key()));
            if (Object == nullptr || !Iterator.value().is_array())
            {
                continue;
            }
            for (const nlohmann::json& ComponentEntry : Iterator.value())
            {
                PrefabGraphComponent Component;
                std::string Error;
                if (!ParseComponent(ComponentEntry, Component, Error))
                {
                    PrintString("Prefab: skipping invalid added component: " + Error);
                    continue;
                }
                Component.InstanceId.clear();
                Object->Components.push_back(std::move(Component));
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Diff
// ----------------------------------------------------------------------------

nlohmann::json PrefabGraphOps::ComputeModifications(
    const PrefabGraph& Base,
    const std::string& InstanceId,
    const std::vector<PrefabGraphObject>& LiveOwnedObjects)
{
    auto Map = [&InstanceId](const std::string& Id)
    {
        return PrefabIds::Combine(InstanceId, Id);
    };

    std::unordered_map<std::string, const PrefabGraphObject*> LiveById;
    for (const PrefabGraphObject& Live : LiveOwnedObjects)
    {
        LiveById[Live.Id] = &Live;
    }

    nlohmann::json ObjectMods = nlohmann::json::object();
    nlohmann::json ComponentMods = nlohmann::json::object();
    nlohmann::json ParentMods = nlohmann::json::object();
    nlohmann::json RemovedObjects = nlohmann::json::array();
    nlohmann::json RemovedComponents = nlohmann::json::array();
    nlohmann::json AddedComponents = nlohmann::json::object();

    for (const PrefabGraphObject& Source : Base.Objects)
    {
        const std::string HostId = Map(Source.Id);
        const auto LiveFound = LiveById.find(HostId);
        if (LiveFound == LiveById.end())
        {
            if (Source.Id == Base.RootId)
            {
                continue;
            }
            // Record only the top-most removed object; descendants are removed implicitly.
            const bool bParentAlive = Source.ParentId.empty()
                || LiveById.find(Map(Source.ParentId)) != LiveById.end();
            if (bParentAlive)
            {
                RemovedObjects.push_back(Source.Id);
            }
            continue;
        }
        const PrefabGraphObject& Live = *LiveFound->second;

        nlohmann::json Fields = nlohmann::json::object();
        if (Live.Name != Source.Name)
        {
            Fields["name"] = Live.Name;
        }
        if (Live.bActive != Source.bActive)
        {
            Fields["bActive"] = Live.bActive;
        }
        if (Live.bVisual != Source.bVisual)
        {
            Fields["bVisual"] = Live.bVisual;
        }
        if (!SceneJson::NearlyEqual(Live.LocalTransform.Position, Source.LocalTransform.Position))
        {
            Fields["position"] = SceneJson::Vector3ToJson(Live.LocalTransform.Position);
        }
        if (!SceneJson::NearlyEqual(Live.LocalTransform.Rotation, Source.LocalTransform.Rotation))
        {
            Fields["rotation"] = SceneJson::QuaternionToJson(Live.LocalTransform.Rotation);
        }
        if (!SceneJson::NearlyEqual(Live.LocalTransform.Scale, Source.LocalTransform.Scale))
        {
            Fields["scale"] = SceneJson::Vector3ToJson(Live.LocalTransform.Scale);
        }
        if (!Fields.empty())
        {
            ObjectMods[Source.Id] = std::move(Fields);
        }

        if (Source.Id != Base.RootId)
        {
            const std::string ExpectedParent = Source.ParentId.empty() ? std::string{} : Map(Source.ParentId);
            if (Live.ParentId != ExpectedParent)
            {
                nlohmann::json Reference = nlohmann::json::object();
                if (LiveById.find(Live.ParentId) != LiveById.end())
                {
                    Reference["source"] = Map(Live.ParentId);
                }
                else
                {
                    Reference["object"] = Live.ParentId;
                }
                ParentMods[Source.Id] = std::move(Reference);
            }
        }

        std::unordered_map<std::string, const PrefabGraphComponent*> LiveComponents;
        for (const PrefabGraphComponent& Component : Live.Components)
        {
            LiveComponents[Component.Id] = &Component;
        }
        std::unordered_set<std::string> MatchedComponents;
        for (const PrefabGraphComponent& SourceComponent : Source.Components)
        {
            const std::string HostComponentId = Map(SourceComponent.Id);
            const auto Found = LiveComponents.find(HostComponentId);
            if (Found == LiveComponents.end() || Found->second->Type != SourceComponent.Type)
            {
                RemovedComponents.push_back(SourceComponent.Id);
                continue;
            }
            MatchedComponents.insert(HostComponentId);
            const PrefabGraphComponent& LiveComponent = *Found->second;
            nlohmann::json ComponentFields = nlohmann::json::object();
            if (LiveComponent.bEnabled != SourceComponent.bEnabled)
            {
                ComponentFields["bEnabled"] = LiveComponent.bEnabled;
            }
            nlohmann::json ChangedProperties = nlohmann::json::object();
            nlohmann::json ExpectedProperties = SourceComponent.Properties;
            RemapObjectReferences(ExpectedProperties, Map);
            for (auto Property = LiveComponent.Properties.begin(); Property != LiveComponent.Properties.end(); ++Property)
            {
                const auto SourceProperty = ExpectedProperties.find(Property.key());
                if (SourceProperty == ExpectedProperties.end()
                    || !SceneJson::NearlyEqual(Property.value(), *SourceProperty))
                {
                    ChangedProperties[Property.key()] = Property.value();
                }
            }
            if (!ChangedProperties.empty())
            {
                ComponentFields["properties"] = std::move(ChangedProperties);
            }
            if (!ComponentFields.empty())
            {
                ComponentMods[SourceComponent.Id] = std::move(ComponentFields);
            }
        }

        nlohmann::json Added = nlohmann::json::array();
        for (const PrefabGraphComponent& LiveComponent : Live.Components)
        {
            if (MatchedComponents.count(LiveComponent.Id) != 0)
            {
                continue;
            }
            if (LiveComponent.InstanceId == InstanceId)
            {
                // Owned by the instance but no longer present in the prefab: dropped on next sync.
                continue;
            }
            Added.push_back(WriteComponent(LiveComponent));
        }
        if (!Added.empty())
        {
            AddedComponents[Source.Id] = std::move(Added);
        }
    }

    nlohmann::json Result = nlohmann::json::object();
    if (!ObjectMods.empty())
    {
        Result["objects"] = std::move(ObjectMods);
    }
    if (!ComponentMods.empty())
    {
        Result["components"] = std::move(ComponentMods);
    }
    if (!ParentMods.empty())
    {
        Result["parents"] = std::move(ParentMods);
    }
    if (!RemovedObjects.empty())
    {
        Result["removedObjects"] = std::move(RemovedObjects);
    }
    if (!RemovedComponents.empty())
    {
        Result["removedComponents"] = std::move(RemovedComponents);
    }
    if (!AddedComponents.empty())
    {
        Result["addedComponents"] = std::move(AddedComponents);
    }
    return Result;
}

bool PrefabGraphOps::IsEmptyModifications(const nlohmann::json& Modifications)
{
    if (!Modifications.is_object())
    {
        return true;
    }
    for (auto Iterator = Modifications.begin(); Iterator != Modifications.end(); ++Iterator)
    {
        if (!Iterator.value().empty())
        {
            return false;
        }
    }
    return true;
}

std::vector<PrefabOverride> PrefabGraphOps::DescribeModifications(
    const PrefabGraph& Base,
    const std::string& InstanceId,
    const nlohmann::json& Modifications)
{
    std::vector<PrefabOverride> Result;
    if (!Modifications.is_object())
    {
        return Result;
    }

    auto Map = [&InstanceId](const std::string& Id)
    {
        return PrefabIds::Combine(InstanceId, Id);
    };
    auto ObjectName = [&Base](const std::string& SourceId)
    {
        const PrefabGraphObject* Object = Base.FindObject(SourceId);
        return Object != nullptr ? Object->Name : SourceId;
    };
    std::unordered_map<std::string, std::pair<const PrefabGraphObject*, const PrefabGraphComponent*>> ComponentsById;
    for (const PrefabGraphObject& Object : Base.Objects)
    {
        for (const PrefabGraphComponent& Component : Object.Components)
        {
            ComponentsById[Component.Id] = {&Object, &Component};
        }
    }

    if (Modifications.contains("objects") && Modifications.at("objects").is_object())
    {
        for (auto Iterator = Modifications.at("objects").begin(); Iterator != Modifications.at("objects").end(); ++Iterator)
        {
            if (!Iterator.value().is_object())
            {
                continue;
            }
            for (auto Field = Iterator.value().begin(); Field != Iterator.value().end(); ++Field)
            {
                PrefabOverride Override;
                Override.Kind = PrefabOverrideKind::ObjectProperty;
                Override.SourceObjectId = Iterator.key();
                Override.HostObjectId = Map(Iterator.key());
                Override.Property = Field.key();
                Override.DisplayText = ObjectName(Iterator.key()) + " · " + Field.key();
                Result.push_back(std::move(Override));
            }
        }
    }

    if (Modifications.contains("components") && Modifications.at("components").is_object())
    {
        for (auto Iterator = Modifications.at("components").begin(); Iterator != Modifications.at("components").end(); ++Iterator)
        {
            const auto Found = ComponentsById.find(Iterator.key());
            if (Found == ComponentsById.end() || !Iterator.value().is_object())
            {
                continue;
            }
            const std::string Prefix = Found->second.first->Name + " · " + ShortTypeName(Found->second.second->Type) + ".";
            auto AddEntry = [&](const std::string& Property)
            {
                PrefabOverride Override;
                Override.Kind = PrefabOverrideKind::ComponentProperty;
                Override.SourceObjectId = Found->second.first->Id;
                Override.SourceComponentId = Iterator.key();
                Override.HostObjectId = Map(Found->second.first->Id);
                Override.HostComponentId = Map(Iterator.key());
                Override.Property = Property;
                Override.DisplayText = Prefix + Property;
                Result.push_back(std::move(Override));
            };
            if (Iterator.value().contains("bEnabled"))
            {
                AddEntry("bEnabled");
            }
            if (Iterator.value().contains("properties") && Iterator.value().at("properties").is_object())
            {
                for (auto Property = Iterator.value().at("properties").begin();
                     Property != Iterator.value().at("properties").end();
                     ++Property)
                {
                    AddEntry(Property.key());
                }
            }
        }
    }

    if (Modifications.contains("addedComponents") && Modifications.at("addedComponents").is_object())
    {
        for (auto Iterator = Modifications.at("addedComponents").begin(); Iterator != Modifications.at("addedComponents").end(); ++Iterator)
        {
            if (!Iterator.value().is_array())
            {
                continue;
            }
            for (const nlohmann::json& ComponentEntry : Iterator.value())
            {
                if (!ComponentEntry.is_object() || !ComponentEntry.contains("id") || !ComponentEntry.at("id").is_string())
                {
                    continue;
                }
                PrefabOverride Override;
                Override.Kind = PrefabOverrideKind::AddedComponent;
                Override.SourceObjectId = Iterator.key();
                Override.HostObjectId = Map(Iterator.key());
                Override.HostComponentId = ComponentEntry.at("id").get<std::string>();
                const std::string Type = ComponentEntry.contains("type") && ComponentEntry.at("type").is_string()
                    ? ComponentEntry.at("type").get<std::string>()
                    : std::string("Component");
                Override.DisplayText = ObjectName(Iterator.key()) + " · + " + ShortTypeName(Type);
                Result.push_back(std::move(Override));
            }
        }
    }

    if (Modifications.contains("removedComponents") && Modifications.at("removedComponents").is_array())
    {
        for (const nlohmann::json& Value : Modifications.at("removedComponents"))
        {
            if (!Value.is_string())
            {
                continue;
            }
            const auto Found = ComponentsById.find(Value.get<std::string>());
            if (Found == ComponentsById.end())
            {
                continue;
            }
            PrefabOverride Override;
            Override.Kind = PrefabOverrideKind::RemovedComponent;
            Override.SourceObjectId = Found->second.first->Id;
            Override.SourceComponentId = Value.get<std::string>();
            Override.HostObjectId = Map(Found->second.first->Id);
            Override.HostComponentId = Map(Value.get<std::string>());
            Override.DisplayText = Found->second.first->Name + " · - " + ShortTypeName(Found->second.second->Type);
            Result.push_back(std::move(Override));
        }
    }

    if (Modifications.contains("removedObjects") && Modifications.at("removedObjects").is_array())
    {
        for (const nlohmann::json& Value : Modifications.at("removedObjects"))
        {
            if (!Value.is_string() || Base.FindObject(Value.get<std::string>()) == nullptr)
            {
                continue;
            }
            PrefabOverride Override;
            Override.Kind = PrefabOverrideKind::RemovedObject;
            Override.SourceObjectId = Value.get<std::string>();
            Override.HostObjectId = Map(Value.get<std::string>());
            Override.DisplayText = "- " + ObjectName(Value.get<std::string>());
            Result.push_back(std::move(Override));
        }
    }

    if (Modifications.contains("parents") && Modifications.at("parents").is_object())
    {
        for (auto Iterator = Modifications.at("parents").begin(); Iterator != Modifications.at("parents").end(); ++Iterator)
        {
            if (Base.FindObject(Iterator.key()) == nullptr)
            {
                continue;
            }
            PrefabOverride Override;
            Override.Kind = PrefabOverrideKind::ReparentedObject;
            Override.SourceObjectId = Iterator.key();
            Override.HostObjectId = Map(Iterator.key());
            Override.DisplayText = ObjectName(Iterator.key()) + " · parent";
            Result.push_back(std::move(Override));
        }
    }
    return Result;
}

void PrefabGraphOps::EraseOverride(nlohmann::json& Modifications, const PrefabOverride& Override)
{
    if (!Modifications.is_object())
    {
        return;
    }
    auto EraseFromArray = [&Modifications](const char* Section, const std::string& Value)
    {
        if (!Modifications.contains(Section) || !Modifications.at(Section).is_array())
        {
            return;
        }
        nlohmann::json& Array = Modifications.at(Section);
        for (auto Iterator = Array.begin(); Iterator != Array.end();)
        {
            if (Iterator->is_string() && Iterator->get<std::string>() == Value)
            {
                Iterator = Array.erase(Iterator);
            }
            else
            {
                ++Iterator;
            }
        }
    };

    switch (Override.Kind)
    {
    case PrefabOverrideKind::ObjectProperty:
        if (Modifications.contains("objects") && Modifications.at("objects").contains(Override.SourceObjectId))
        {
            Modifications.at("objects").at(Override.SourceObjectId).erase(Override.Property);
        }
        break;
    case PrefabOverrideKind::ComponentProperty:
        if (Modifications.contains("components") && Modifications.at("components").contains(Override.SourceComponentId))
        {
            nlohmann::json& Fields = Modifications.at("components").at(Override.SourceComponentId);
            if (Override.Property == "bEnabled")
            {
                Fields.erase("bEnabled");
            }
            else if (Fields.contains("properties"))
            {
                Fields.at("properties").erase(Override.Property);
            }
        }
        break;
    case PrefabOverrideKind::AddedComponent:
        if (Modifications.contains("addedComponents") && Modifications.at("addedComponents").contains(Override.SourceObjectId))
        {
            nlohmann::json& Array = Modifications.at("addedComponents").at(Override.SourceObjectId);
            for (auto Iterator = Array.begin(); Iterator != Array.end();)
            {
                if (Iterator->is_object() && Iterator->value("id", std::string{}) == Override.HostComponentId)
                {
                    Iterator = Array.erase(Iterator);
                }
                else
                {
                    ++Iterator;
                }
            }
        }
        break;
    case PrefabOverrideKind::RemovedComponent:
        EraseFromArray("removedComponents", Override.SourceComponentId);
        break;
    case PrefabOverrideKind::RemovedObject:
        EraseFromArray("removedObjects", Override.SourceObjectId);
        break;
    case PrefabOverrideKind::ReparentedObject:
        if (Modifications.contains("parents"))
        {
            Modifications.at("parents").erase(Override.SourceObjectId);
        }
        break;
    case PrefabOverrideKind::AddedObject:
        break;
    }
}

void PrefabGraphOps::RemapObjectReferences(nlohmann::json& Value, const std::function<std::string(const std::string&)>& Mapper)
{
    if (Value.is_object() && Value.size() == 1 && Value.contains("object") && Value["object"].is_string())
    {
        const std::string Identifier = Value["object"].get<std::string>();
        if (!Identifier.empty())
        {
            Value["object"] = Mapper(Identifier);
        }
        return;
    }
    if (Value.is_object() || Value.is_array())
    {
        for (auto& Child : Value)
        {
            RemapObjectReferences(Child, Mapper);
        }
    }
}
