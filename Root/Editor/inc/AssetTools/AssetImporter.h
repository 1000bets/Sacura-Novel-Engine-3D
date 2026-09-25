#pragma once

#include "AssetTools/ImportRequest.h"
#include "AssetTools/ImportResult.h"
#include "Assets/AssetRegistry.h"

class AssetImporter
{
public:
    explicit AssetImporter(AssetRegistry& Registry);

    ImportResult Import(const ImportRequest& Request);

private:
    ImportResult ImportGlb(const ImportRequest& Request);
    ImportResult ImportImage(const ImportRequest& Request);
    ImportResult ImportFbx(const ImportRequest& Request);
    std::vector<SubAssetRecord> BuildUniqueSubAssets(
        const std::string& DestinationRelativePath,
        const std::vector<SubAssetRecord>& SourceSubAssets) const;

    ImportResult PublishPair(
        const ImportRequest& Request,
        const std::filesystem::path& StagedDataPath,
        AssetType Type,
        const nlohmann::json& ImportInfo,
        const std::vector<SubAssetRecord>& SubAssets = {});

    AssetRegistry& Registry;
};
