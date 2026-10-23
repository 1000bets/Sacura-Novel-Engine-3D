#pragma once
#include "Assets/AssetRegistry.h"
#include "Assets/StaticMeshDefaults.h"

class AssetManager;
class StaticMeshDocument
{
public:
    explicit StaticMeshDocument(AssetRegistry& Registry);
    bool Open(const AssetKey& Asset, std::string& Error);
    bool Edit(const nlohmann::json& Value, std::string& Error);
    bool Undo();
    bool Redo();
    bool Save(AssetManager& Manager, std::string& Error);
    bool IsDirty() const;
    const AssetKey& GetAsset() const { return Asset; }
    const StaticMeshDefaults& GetDefaults() const { return Defaults; }
    const AssetRegistryEntry& GetEntry() const { return Entry; }
private:
    AssetRegistry& Registry;
    AssetRegistryEntry Entry;
    AssetKey Asset;
    StaticMeshDefaults Defaults;
    nlohmann::json Saved;
    std::vector<nlohmann::json> History;
    size_t Position = 0;
};
