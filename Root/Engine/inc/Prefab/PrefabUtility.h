#pragma once

#include "Prefab/PrefabTypes.h"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

class GameObject;
class Scene;

enum class PrefabUnpackMode : uint8_t
{
    // Removes only the outermost link; nested prefabs (and a variant's base) become instances.
    OutermostRoot = 0,
    // Removes every prefab link in the subtree; all objects become plain scene objects.
    Completely
};

// High-level prefab API modeled on Unity's PrefabUtility. Engine-side and UI-independent:
// used by the editor, by tests and (for InstantiatePrefab) by runtime/gameplay code.
// Every function runs on the Game thread.
class PrefabUtility
{
public:
    // ---------------------------------------------------------------- queries
    static bool IsPartOfPrefabInstance(const GameObject* Object);
    static bool IsOutermostInstanceRoot(const GameObject* Object);
    // Plain object whose parent belongs to a prefab instance (Unity "added GameObject").
    static bool IsAddedObject(const GameObject* Object);
    static GameObject* GetOutermostInstanceRoot(const GameObject* Object);
    // Root of a nested prefab inside the outermost prefab (e.g. a Wheel inside a Car instance).
    static bool IsNestedPrefabRoot(const GameObject* Object, AssetId* OutNestedPrefab = nullptr);
    static AssetId GetPrefabAsset(const GameObject* InstanceRoot);

    // ---------------------------------------------------------------- instantiation
    static GameObject* InstantiatePrefab(
        Scene& TargetScene,
        const AssetId& Prefab,
        GameObject* Parent,
        std::string& OutError);

    // ---------------------------------------------------------------- overrides
    static bool GetOverrides(
        Scene& SourceScene,
        GameObject& InstanceRoot,
        std::vector<PrefabOverride>& OutOverrides,
        std::string& OutError);
    // Keys "<hostObjectId>/<field>", "<hostComponentId>/<property>", "<hostComponentId>/*" (added
    // component) and "<hostObjectId>/*" (added object) for UI highlighting.
    static void CollectOverrideMarkers(
        Scene& SourceScene,
        GameObject& InstanceRoot,
        std::unordered_set<std::string>& OutMarkers);

    static bool RevertOverrides(
        Scene& TargetScene,
        GameObject& InstanceRoot,
        const std::vector<PrefabOverride>& Overrides,
        std::string& OutError);
    static bool RevertAll(Scene& TargetScene, GameObject& InstanceRoot, std::string& OutError);

    // Writes the selected overrides (all when Selection is empty) into the outermost prefab asset,
    // saves it and synchronizes every instance of that prefab in TargetScene. Other open scenes are
    // updated through PrefabLibrary change listeners.
    static bool ApplyOverrides(
        Scene& TargetScene,
        GameObject& InstanceRoot,
        const std::vector<PrefabOverride>& Selection,
        std::string& OutError);

    // ---------------------------------------------------------------- structure
    static bool Unpack(Scene& TargetScene, GameObject& InstanceRoot, PrefabUnpackMode Mode, std::string& OutError);

    // Creates a *.prefab from a live subtree and turns the subtree into an instance of it (ids are
    // preserved). If Root is an instance root: bAsVariant=true makes a variant of its prefab,
    // otherwise the root link is unpacked first ("Original Prefab").
    static bool CreatePrefabAsset(
        Scene& TargetScene,
        GameObject& Root,
        const std::filesystem::path& AbsolutePath,
        bool bAsVariant,
        AssetId& OutPrefab,
        std::string& OutError);
    static bool CreateVariantAsset(
        const AssetId& BasePrefab,
        const std::filesystem::path& AbsolutePath,
        AssetId& OutPrefab,
        std::string& OutError);

    // ---------------------------------------------------------------- documents / propagation
    // Synchronizes instances of ChangedPrefab (or of every prefab when invalid). Returns the count.
    static int RefreshInstances(Scene& TargetScene, const AssetId& ChangedPrefab, std::string& OutErrors);

    // Loads a prefab document into an empty scene (prefab editor / apply). Ids stay in prefab space.
    static bool LoadPrefabIntoScene(const AssetId& Prefab, Scene& TargetScene, GameObject*& OutRoot, std::string& OutError);
    static bool BuildPrefabDocument(const Scene& SourceScene, const std::string& Name, nlohmann::json& OutDocument, std::string& OutError);
    // Saves a single-root scene into an existing prefab asset (rejects nesting cycles) and notifies.
    static bool SavePrefabFromScene(const Scene& SourceScene, const AssetId& Prefab, std::string& OutError);
    // Writes a new asset file under Game Content and registers it.
    static bool SavePrefabFromSceneAs(
        const Scene& SourceScene,
        const std::filesystem::path& AbsolutePath,
        AssetId& OutPrefab,
        std::string& OutError);

    static bool RegisterPrefabFile(const std::filesystem::path& AbsolutePath, AssetId& OutPrefab, std::string& OutError);
    static bool ValidateNewPrefabPath(const std::filesystem::path& AbsolutePath, std::string& OutError);
};
