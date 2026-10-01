#pragma once

#include "Assets/AssetTypes.h"
#include "Core/Transform.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// Prefab system — data model
// ============================================================================
//
// A prefab asset (*.prefab, "sakura.prefab" v1) stores exactly the same entry
// list as a scene document, but with a single root entry:
//
//   * plain object entry        — { "id", "name", "parent", "transform", "components", ... }
//   * prefab instance entry     — { "kind": "prefabInstance", "id", "prefab", "parent",
//                                   "parentSource"?, "modifications" }
//
// A regular prefab has a plain root. A prefab VARIANT is a prefab whose root entry
// is an instance of its base prefab (Unity model). A NESTED prefab is an instance
// entry anywhere below the root.
//
// Identification (Unity "fileID XOR prefabInstanceID" model):
//   * every object/component of a prefab has a stable GUID in that prefab's id space;
//   * an instance with record id I produces the object Combine(I, SourceId)
//     where Combine is a byte-wise XOR of the canonical GUIDs;
//   * XOR is associative, so Combine(I, Combine(J, q)) == Combine(Combine(I, J), q):
//     nested ids never depend on how the nesting chain is grouped, which makes
//     "unpack one level" and "create prefab from instance" id-preserving.
//
// Overrides are stored per outermost instance, keyed by SourceId (prefab space).
// They are *computed* by diffing the live instance against the prefab snapshot the
// instance was last synchronized with, so every existing editor command (rename,
// set transform, set property, add/remove component, reparent, delete...) produces
// overrides without knowing about prefabs.

inline constexpr const char* PrefabDocumentFormatId = "sakura.prefab";
inline constexpr uint32_t PrefabDocumentFormatVersion = 1;
inline constexpr const char* PrefabInstanceEntryKind = "prefabInstance";
inline constexpr const char* PrefabFileExtension = ".prefab";

namespace PrefabIds
{
// Returns the lowercase GUID text for GUID ids, or a deterministic GUID derived from
// the text (128-bit FNV-1a) for legacy ids such as "obj_1".
std::string Canonicalize(const std::string& Id);
// XOR of the canonical GUIDs of A and B. Associative and self-inverse:
// Combine(Combine(A, B), B) == Canonicalize(A).
std::string Combine(const std::string& A, const std::string& B);
std::string Generate();
}

struct PrefabGraphComponent
{
    std::string Id;
    std::string Type;
    uint32_t TypeVersion = 1;
    bool bEnabled = true;
    nlohmann::json Properties = nlohmann::json::object();
    // Outermost prefab instance (in the graph's own id space) that provides this component.
    // Empty for components that are local to the document (plain objects, added components).
    std::string InstanceId;
};

struct PrefabGraphObject
{
    std::string Id;
    std::string Name;
    bool bActive = true;
    bool bVisual = true;
    Transform LocalTransform{};
    std::string ParentId;
    std::vector<PrefabGraphComponent> Components;
    // Outermost prefab instance (in the graph's own id space) that provides this object.
    std::string InstanceId;
};

struct PrefabGraph;

// One prefab instance record inside a graph / live scene.
struct PrefabGraphInstance
{
    std::string InstanceId;
    AssetId Prefab{};
    std::string RootObjectId;
    nlohmann::json Modifications = nlohmann::json::object();
    // Resolved prefab graph (prefab id space) the instance is based on.
    // Null when the prefab asset is missing or failed to resolve.
    std::shared_ptr<const PrefabGraph> Base;
    // When the prefab is missing, the original record is kept verbatim so that
    // saving the document does not lose data.
    nlohmann::json MissingRecord;
    std::unordered_map<std::string, std::string> MissingChildParentSources;
};

// Fully expanded object graph. For a resolved prefab the ids are in that prefab's
// id space; for a scene document they are scene persistent ids.
struct PrefabGraph
{
    std::string RootId;
    std::vector<PrefabGraphObject> Objects;
    std::vector<PrefabGraphInstance> Instances;
    // Every prefab asset this graph (transitively) depends on.
    std::vector<AssetId> Dependencies;

    const PrefabGraphObject* FindObject(const std::string& Id) const;
    PrefabGraphObject* FindObject(const std::string& Id);
    const PrefabGraphInstance* FindInstance(const std::string& InstanceId) const;
    bool DependsOn(const AssetId& Asset) const;
    void RebuildIndex();

private:
    std::unordered_map<std::string, size_t> ObjectIndex;
};

// Attached to the root GameObject of every *outermost* prefab instance in a live scene.
struct PrefabInstanceData
{
    std::string InstanceId;
    AssetId Prefab{};
    std::shared_ptr<const PrefabGraph> Base;
    // Cached last computed / loaded modifications (authoritative only when Base is null).
    nlohmann::json Modifications = nlohmann::json::object();
    nlohmann::json MissingRecord;
    // For a missing prefab: child object id -> original "parentSource" of that child.
    std::unordered_map<std::string, std::string> MissingChildParentSources;

    bool IsMissing() const { return Base == nullptr; }
};

// Human-readable, individually revertable/applicable override (Unity "Overrides" dropdown).
enum class PrefabOverrideKind : uint8_t
{
    ObjectProperty = 0,   // name / bActive / bVisual / position / rotation / scale
    ComponentProperty,    // reflected property or bEnabled of a prefab component
    AddedComponent,
    RemovedComponent,
    RemovedObject,
    ReparentedObject,
    AddedObject           // local object (or nested instance) placed under an instance object
};

struct PrefabOverride
{
    PrefabOverrideKind Kind = PrefabOverrideKind::ObjectProperty;
    // Source ids are in the prefab id space; host ids are live scene persistent ids.
    std::string SourceObjectId;
    std::string SourceComponentId;
    std::string HostObjectId;
    std::string HostComponentId;
    // "name", "bActive", "bVisual", "position", "rotation", "scale", "bEnabled" or a reflected PropertyId.
    std::string Property;
    std::string DisplayText;
};
