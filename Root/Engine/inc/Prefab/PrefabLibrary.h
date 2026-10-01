#pragma once

#include "Prefab/PrefabTypes.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class AssetRegistry;

// Game-thread service that turns prefab asset ids into resolved (fully expanded) graphs.
//
//  * Reads "sakura.prefab" documents through AssetRegistry (GUID -> absolute path).
//  * Resolves variants and nested prefabs recursively with cycle detection.
//  * Caches resolved graphs; a cache entry is validated against the file write time of the
//    prefab and of every prefab it depends on, so external edits are picked up as well.
//  * NotifyPrefabChanged invalidates dependents and informs listeners (the editor uses it to
//    propagate a saved prefab to every open scene and prefab editor).
//
// Runtime (SakuraPlayer) only uses Resolve through SceneSerializer; no editor code is required.
class PrefabLibrary
{
public:
    using ChangeListener = std::function<void(const AssetId& ChangedPrefab)>;

    static PrefabLibrary& Get();

    void BindRegistry(AssetRegistry* Registry);
    AssetRegistry* GetRegistry() const { return BoundRegistry; }

    bool TryGetPrefabPath(const AssetId& Prefab, std::filesystem::path& OutAbsolutePath) const;
    std::string GetPrefabDisplayName(const AssetId& Prefab) const;

    // Reads and validates a prefab document ("format", "formatVersion", "objects").
    static bool ReadDocumentFile(
        const std::filesystem::path& AbsolutePath,
        nlohmann::json& OutDocument,
        std::string& OutError);
    static bool WriteDocumentFile(
        const std::filesystem::path& AbsolutePath,
        const nlohmann::json& Document,
        std::string& OutError);
    static nlohmann::json MakeDocument(const std::string& Name, nlohmann::json Entries);

    bool ReadDocument(const AssetId& Prefab, nlohmann::json& OutDocument, std::string& OutError) const;

    // Returns the resolved prefab graph (prefab id space) or null with an error.
    std::shared_ptr<const PrefabGraph> Resolve(const AssetId& Prefab, std::string& OutError);
    // Internal: used while expanding nested documents. ResolveStack detects cycles.
    std::shared_ptr<const PrefabGraph> ResolveWithStack(
        const AssetId& Prefab,
        std::vector<AssetId>& ResolveStack,
        std::string& OutError);

    // True when Prefab == Candidate or Prefab (transitively) contains / derives from Candidate.
    bool DependsOn(const AssetId& Prefab, const AssetId& Candidate);

    void ClearCache();
    void Invalidate(const AssetId& ChangedPrefab);
    void NotifyPrefabChanged(const AssetId& ChangedPrefab);

    uint64_t AddChangeListener(ChangeListener Listener);
    void RemoveChangeListener(uint64_t ListenerId);

private:
    PrefabLibrary() = default;

    struct CacheEntry
    {
        std::shared_ptr<const PrefabGraph> Graph;
        std::unordered_map<std::string, std::filesystem::file_time_type> FileTimes;
    };

    bool IsCacheEntryFresh(const CacheEntry& Entry) const;

    AssetRegistry* BoundRegistry = nullptr;
    std::unordered_map<AssetId, CacheEntry, GuidHash> Cache;
    std::unordered_map<uint64_t, ChangeListener> Listeners;
    uint64_t NextListenerId = 1;
    bool bNotifying = false;
};
