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
    std::string RelativePath;
    std::string AbsolutePath;
    std::string AbsoluteMetaPath;
    bool bRegistered = true;
};

class AssetRegistry
{
public:
    void SetContentRoot(const std::filesystem::path& ContentRoot);
    const std::filesystem::path& GetContentRoot() const { return ContentRoot; }

    void Clear();
    AssetDiagnostic ScanContent();

    AssetDiagnostic RegisterExistingAsset(const std::string& RelativePath, AssetMetadata Metadata);
    AssetDiagnostic Unregister(const AssetId& Id);

    bool TryGetById(const AssetId& Id, AssetRegistryEntry& OutEntry) const;
    bool TryGetByPath(const std::string& RelativePath, AssetRegistryEntry& OutEntry) const;
    bool TryResolveKey(const AssetKey& Key, AssetRegistryEntry& OutEntry, const SubAssetRecord** OutSubAsset = nullptr) const;

    bool Exists(const AssetId& Id) const;
    bool PathExists(const std::string& RelativePath) const;

    std::vector<AssetRegistryEntry> FindByType(AssetType Type) const;
    std::vector<AssetRegistryEntry> FindByDirectory(const std::string& RelativeDirectory) const;

    AssetDiagnostic RenamePair(const std::string& OldRelativePath, const std::string& NewRelativePath);

    const std::vector<AssetDiagnostic>& GetScanDiagnostics() const { return ScanDiagnostics; }

private:
    AssetDiagnostic IngestMetaFile(const std::filesystem::path& AbsoluteMetaPath);
    static AssetType InferTypeFromExtension(const std::string& RelativePath);

    std::filesystem::path ContentRoot;
    std::unordered_map<AssetId, AssetRegistryEntry, GuidHash> EntriesById;
    std::unordered_map<std::string, AssetId> IdsByRelativePath;
    std::vector<AssetDiagnostic> ScanDiagnostics;
};
