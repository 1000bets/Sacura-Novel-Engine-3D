#pragma once

#include "Prefab/PrefabTypes.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Component;
class GameObject;
class Object;
class Scene;

// Live-scene side of the prefab system: builds GameObjects from expanded graphs, writes
// document entries (collapsing instances back into records) and synchronizes existing
// instances in place when their prefab changes.
//
// It is a friend of Object so it can assign persistent ids, like SceneSerializer.
class PrefabSceneBuilder
{
public:
    static void SetPersistentId(Object& Target, const std::string& PersistentId);

    static void CollectSubtree(GameObject* Root, std::vector<GameObject*>& OutObjects);
    // Depth-first subtree that does not descend into objects owned by StopAtInstanceId.
    static void CollectSubtreeExcludingInstance(
        GameObject* Root,
        const std::string& StopAtInstanceId,
        std::vector<GameObject*>& OutObjects);

    static bool CaptureObject(const GameObject& Source, PrefabGraphObject& OutObject, std::string& OutError);
    static bool CaptureComponent(const Component& Source, PrefabGraphComponent& OutComponent, std::string& OutError);
    static bool CaptureInstance(
        const Scene& SourceScene,
        const std::string& InstanceId,
        std::vector<PrefabGraphObject>& OutObjects,
        std::string& OutError);

    static GameObject* FindInstanceRoot(const Scene& SourceScene, const std::string& InstanceId);
    static Component* FindComponent(const Scene& SourceScene, const std::string& ComponentPersistentId);

    // Creates a component from graph data (not attached). Caller owns it on success.
    static bool CreateComponent(const PrefabGraphComponent& Data, Component*& OutComponent, std::string& OutError, Scene* World = nullptr, bool bDeferProperties = false);
    static void ApplyObjectField(GameObject& Target, const std::string& Field, const nlohmann::json& Value);

    // Creates live objects for every graph object and attaches instance data to instance roots.
    // Graph roots are parented to OptionalParent. All-or-nothing: on failure nothing remains.
    static bool Materialize(
        Scene& TargetScene,
        const PrefabGraph& Graph,
        GameObject* OptionalParent,
        std::vector<GameObject*>& OutRoots,
        std::string& OutError);

    // Writes document entries for Objects. Instances whose root is in Objects are collapsed into
    // "prefabInstance" records with computed modifications. In subtree mode, parents outside the
    // set are written as null and objects of partially included instances keep a "prefabOwner" tag.
    static bool WriteEntries(
        const Scene& SourceScene,
        const std::vector<GameObject*>& Objects,
        bool bSubtree,
        nlohmann::json& OutEntries,
        bool& bOutHasInstances,
        std::string& OutError);

    // Rewrites every object/component id of a subtree document through Mapper (strings equal to a
    // known id are replaced everywhere except in prefab-space fields such as "parentSource").
    // Ownership tags are dropped: remapped copies are always plain/local objects.
    static bool RemapEntryIds(
        nlohmann::json& Entries,
        const std::function<std::string(const std::string&)>& Mapper,
        std::string& OutError);

    // Live modifications of an outermost instance (diff against its base snapshot).
    static bool ComputeLiveModifications(
        const Scene& SourceScene,
        const GameObject& InstanceRoot,
        nlohmann::json& OutModifications,
        std::string& OutError);

    // Brings a live instance to NewBase + modifications, preserving object identity where possible.
    // ForcedModifications == nullptr keeps the current overrides (computed against the old base).
    static bool ReconcileInstance(
        Scene& TargetScene,
        GameObject& InstanceRoot,
        std::shared_ptr<const PrefabGraph> NewBase,
        const nlohmann::json* ForcedModifications,
        std::string& OutError);

private:
    static bool SyncLiveObjects(
        Scene& TargetScene,
        const std::string& InstanceId,
        const std::vector<PrefabGraphObject>& Desired,
        GameObject& InstanceRoot,
        GameObject* RootParent,
        std::string& OutError);
    static void SyncComponents(GameObject& Target, const PrefabGraphObject& Desired, bool bApplyProperties);
};
