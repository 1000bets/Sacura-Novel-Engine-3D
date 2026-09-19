#pragma once

#include "Assets/AssetMetadata.h"
#include "Assets/AssetPath.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct AssetRegistryEntry
{
    AssetMetadata Metadata{};
    AssetMount Mount = AssetMount::Game;
    std::string RelativePath;
    std::string VirtualPath;
    std::string AbsolutePath;
    std::string AbsoluteMetaPath;
    bool bRegistered = true;
};

class AssetRegistry
{
public:
    void SetContentRoot(const std::filesystem::path& ContentRoot);
    void SetGameContentRoot(const std::filesystem::path& ContentRoot);
    void SetEngineContentRoot(const std::filesystem::path& ContentRoot);

    const std::filesystem::path& GetContentRoot() const { return GameContentRoot; }
    const std::filesystem::path& GetGameContentRoot() const { return GameContentRoot; }
    const std::filesystem::path& GetEngineContentRoot() const { return EngineContentRoot; }

    void Clear();
    AssetDiagnostic ScanContent();

    AssetDiagnostic RegisterExistingAsset(const std::string& RelativeOrVirtualPath, AssetMetadata Metadata);
    AssetDiagnostic Unregister(const AssetId& Id);

    bool TryGetById(const AssetId& Id, AssetRegistryEntry& OutEntry) const;
    bool TryGetByPath(const std::string& RelativeOrVirtualPath, AssetRegistryEntry& OutEntry) const;
    bool TryResolveKey(const AssetKey& Key, AssetRegistryEntry& OutEntry, const SubAssetRecord** OutSubAsset = nullptr) const;

    bool Exists(const AssetId& Id) const;
    bool PathExists(const std::string& RelativeOrVirtualPath) const;

    std::vector<AssetRegistryEntry> FindByType(AssetType Type) const;
    std::vector<AssetRegistryEntry> FindByDirectory(const std::string& RelativeOrVirtualDirectory) const;

    AssetDiagnostic RenamePair(const std::string& OldRelativeOrVirtualPath, const std::string& NewRelativeOrVirtualPath);

    const std::vector<AssetDiagnostic>& GetScanDiagnostics() const { return ScanDiagnostics; }

private:
    AssetDiagnostic IngestMetaFile(AssetMount Mount, const std::filesystem::path& AbsoluteMetaPath);
    AssetDiagnostic ScanMount(AssetMount Mount, const std::filesystem::path& ContentRoot);
    bool ResolvePathRoots(
        const std::string& RelativeOrVirtualPath,
        AssetMount& OutMount,
        std::string& OutRelativeInsideContent,
        std::filesystem::path& OutContentRoot) const;
    static AssetType InferTypeFromExtension(const std::string& RelativePath);

    std::filesystem::path GameContentRoot;
    std::filesystem::path EngineContentRoot;
    std::unordered_map<AssetId, AssetRegistryEntry, GuidHash> EntriesById;
    std::unordered_map<std::string, AssetId> IdsByVirtualPath;
    std::vector<AssetDiagnostic> ScanDiagnostics;
};
