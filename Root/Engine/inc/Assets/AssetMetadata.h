#pragma once

#include "Assets/AssetTypes.h"
#include "Assets/ContentHash.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

struct SubAssetSelector
{
    std::string Kind;
    int32_t Index = -1;
};

struct SubAssetRecord
{
    SubAssetId Id{};
    AssetType Type = AssetType::Unknown;
    std::string Name;
    SubAssetSelector Selector{};
};

struct AssetMetadata
{
    int32_t SchemaVersion = 1;
    AssetId Guid{};
    AssetType Type = AssetType::Unknown;
    nlohmann::json LoadSettings = nlohmann::json::object();
    std::vector<SubAssetRecord> SubAssets;
    ContentHash SourceFingerprint{};
    nlohmann::json ImportInfo = nlohmann::json::object();

    bool IsValid() const { return Guid.IsValid() && Type != AssetType::Unknown; }
};

namespace AssetMetadataIO
{
bool TryLoadFromFile(const std::string& AbsoluteMetaPath, AssetMetadata& OutMetadata, AssetDiagnostic& OutError);
bool TrySaveToFile(const std::string& AbsoluteMetaPath, const AssetMetadata& Metadata, AssetDiagnostic& OutError);
nlohmann::json ToJson(const AssetMetadata& Metadata);
bool TryFromJson(const nlohmann::json& Document, AssetMetadata& OutMetadata, AssetDiagnostic& OutError);

nlohmann::json AssetRefToJson(const AssetKey& Key);
bool TryAssetRefFromJson(const nlohmann::json& Document, AssetKey& OutKey, AssetDiagnostic& OutError);
}
