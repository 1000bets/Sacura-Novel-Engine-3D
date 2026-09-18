#pragma once

#include "Assets/Loaders/IAssetLoader.h"

class MaterialLoader : public IAssetLoader
{
public:
    AssetType GetAssetType() const override { return AssetType::Material; }
    const char* GetLoaderName() const override { return "MaterialLoader"; }

    void CollectDependencies(const AssetLoadContext& Context, std::vector<AssetKey>& OutDependencies) const override;
    std::shared_ptr<const void> Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic) override;
};
