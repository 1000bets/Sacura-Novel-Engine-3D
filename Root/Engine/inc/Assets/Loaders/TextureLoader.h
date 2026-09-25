#pragma once

#include "Assets/Loaders/IAssetLoader.h"

class TextureLoader : public IAssetLoader
{
public:
    AssetType GetAssetType() const override { return TextureAssetType; }
    const char* GetLoaderName() const override { return "TextureLoader"; }

    std::shared_ptr<const void> Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic) override;
};
