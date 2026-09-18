#pragma once

#include "Assets/Loaders/IAssetLoader.h"

class TextureLoader : public IAssetLoader
{
public:
    AssetType GetAssetType() const override { return AssetType::Texture; }
    const char* GetLoaderName() const override { return "TextureLoader"; }

    std::shared_ptr<const void> Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic) override;
};
