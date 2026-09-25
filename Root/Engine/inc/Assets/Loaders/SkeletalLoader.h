#pragma once

#include "Assets/Loaders/IAssetLoader.h"
#include "Assets/Loaders/ModelLoader.h"

class SkeletalLoader : public IAssetLoader
{
public:
    explicit SkeletalLoader(ModelLoader& SharedModelLoader);

    AssetType GetAssetType() const override { return SkeletalMeshAssetType; }
    const char* GetLoaderName() const override { return "SkeletalLoader"; }

    std::shared_ptr<const void> Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic) override;

private:
    ModelLoader& SharedModelLoader;
};
