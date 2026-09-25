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

struct AssetTypeRegistration
{
    AssetType Type = UnknownAssetType;
    std::string DisplayName;
    std::vector<std::string> Extensions;
    bool bVisibleInContentBrowser = true;
};

class AssetRegistry
{
public:
    AssetRegistry();

    void SetContentRoot(const std::filesystem::path& ContentRoot);
    void SetGameContentRoot(const std::filesystem::path& ContentRoot);
    void SetEngineContentRoot(const std::filesystem::path& ContentRoot);

    const std::filesystem::path& GetContentRoot() const { return GameContentRoot; }
    const std::filesystem::path& GetGameContentRoot() const { return GameContentRoot; }
    const std::filesystem::path& GetEngineContentRoot() const { return EngineContentRoot; }

    void Clear();
    AssetDiagnostic ScanContent();

    AssetDiagnostic RegisterAssetType(const AssetTypeRegistration& Registration);
    bool TryGetAssetTypeRegistration(const AssetType& Type, AssetTypeRegistration& OutRegistration) const;
    std::vector<AssetTypeRegistration> GetAssetTypeRegistrations() const;

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
    AssetDiagnostic DeletePair(const std::string& RelativeOrVirtualPath);
    AssetDiagnostic RenameSubAsset(const AssetKey& Key, const std::string& NewName);
    AssetDiagnostic DeleteSubAsset(const AssetKey& Key);
    bool LeafNameExists(const std::string& Name, const AssetKey& IgnoredKey = {}) const;

    const std::vector<AssetDiagnostic>& GetScanDiagnostics() const { return ScanDiagnostics; }

private:
    AssetDiagnostic IngestMetaFile(AssetMount Mount, const std::filesystem::path& AbsoluteMetaPath);
    AssetDiagnostic ScanMount(AssetMount Mount, const std::filesystem::path& ContentRoot);
    bool ResolvePathRoots(
        const std::string& RelativeOrVirtualPath,
        AssetMount& OutMount,
        std::string& OutRelativeInsideContent,
        std::filesystem::path& OutContentRoot) const;
    AssetType InferTypeFromExtension(const std::string& RelativePath) const;

    std::filesystem::path GameContentRoot;
    std::filesystem::path EngineContentRoot;
    std::unordered_map<AssetId, AssetRegistryEntry, GuidHash> EntriesById;
    std::unordered_map<std::string, AssetId> IdsByVirtualPath;
    std::unordered_map<std::string, AssetTypeRegistration> TypeRegistrations;
    std::unordered_map<std::string, AssetType> TypesByExtension;
    std::vector<AssetDiagnostic> ScanDiagnostics;
};
