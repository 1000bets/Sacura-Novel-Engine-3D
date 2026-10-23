#pragma once

#include "Prefab/PrefabTypes.h"

#include <string>
#include <functional>
#include <vector>

// Pure (scene-independent) operations over document entries and resolved graphs.
//
// Modifications JSON (per outermost instance, all keys are SOURCE ids of the prefab):
// {
//   "objects":           { "<srcObject>":    { "name", "bActive", "bVisual", "position", "rotation", "scale" } },
//   "components":        { "<srcComponent>": { "bEnabled": bool, "properties": { "<PropertyId>": value } } },
//   "parents":           { "<srcObject>":    { "source": "<srcObject>" } | { "object": "<hostObjectId>" } },
//   "removedObjects":    [ "<srcObject>" ],          // removing an object removes its prefab subtree
//   "removedComponents": [ "<srcComponent>" ],
//   "addedComponents":   { "<srcObject>":    [ <component entry with a host-local id> ] }
// }
// Added objects are not part of the modifications: they are ordinary document entries whose
// parent is written as { "parent": <instance record id>, "parentSource": <srcObject> }.
class PrefabGraphOps
{
public:
    static void RemapObjectReferences(nlohmann::json& Value, const std::function<std::string(const std::string&)>& Mapper);
    struct ExpandOptions
    {
        // Prefab documents canonicalize every id to a GUID so that XOR derivation is invertible.
        bool bCanonicalizeIds = false;
        // Scenes keep an instance of a missing prefab as a placeholder object (data is preserved).
        bool bAllowMissingPrefabs = true;
    };

    // Expands document entries (plain objects + prefab instance records) into a flat graph.
    static bool Expand(
        const nlohmann::json& Entries,
        const ExpandOptions& Options,
        std::vector<AssetId>& ResolveStack,
        PrefabGraph& OutGraph,
        std::string& OutError);

    // Produces the host-space objects of an instance with record id InstanceId.
    static void Instantiate(
        const PrefabGraph& Base,
        const std::string& InstanceId,
        const nlohmann::json& Modifications,
        std::vector<PrefabGraphObject>& OutObjects,
        std::string& OutRootId);

    // Diff between the prefab snapshot and the live objects owned by the instance.
    static nlohmann::json ComputeModifications(
        const PrefabGraph& Base,
        const std::string& InstanceId,
        const std::vector<PrefabGraphObject>& LiveOwnedObjects);

    static bool IsEmptyModifications(const nlohmann::json& Modifications);

    // Splits modifications into individually revertable/applicable entries (UI).
    static std::vector<PrefabOverride> DescribeModifications(
        const PrefabGraph& Base,
        const std::string& InstanceId,
        const nlohmann::json& Modifications);

    // Removes one override entry from a modifications object (used by Revert).
    static void EraseOverride(nlohmann::json& Modifications, const PrefabOverride& Override);

    // Plain entry / component (de)serialization shared with SceneSerializer.
    static bool ParsePlainEntry(
        const nlohmann::json& Entry,
        PrefabGraphObject& OutObject,
        std::string& OutParentRef,
        std::string& OutParentSource,
        std::string& OutError);
    static nlohmann::json WritePlainEntry(
        const PrefabGraphObject& Object,
        const std::string& ParentRef,
        const std::string& ParentSource);
    static bool ParseComponent(const nlohmann::json& Entry, PrefabGraphComponent& OutComponent, std::string& OutError);
    static nlohmann::json WriteComponent(const PrefabGraphComponent& Component);

    // Object-level fields that can be overridden.
    static const std::vector<std::string>& ObjectFieldNames();
    static nlohmann::json ReadObjectField(const PrefabGraphObject& Object, const std::string& Field);
    static void WriteObjectField(PrefabGraphObject& Object, const std::string& Field, const nlohmann::json& Value);
};
