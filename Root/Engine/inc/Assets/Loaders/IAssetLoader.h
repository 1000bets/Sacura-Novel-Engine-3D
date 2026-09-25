#pragma once

#include "Assets/AssetHandle.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetTypes.h"

#include <memory>
#include <optional>
#include <vector>

struct AssetLoadContext
{
    const AssetRegistry* Registry = nullptr;
    AssetRegistryEntry Entry{};
    std::optional<SubAssetRecord> SubAsset{};
    AssetKey Key{};
};

inline void BindLoadContextSubAsset(AssetLoadContext& Context, const SubAssetRecord* ResolvedSubAsset)
{
    if (ResolvedSubAsset != nullptr)
    {
        Context.SubAsset = *ResolvedSubAsset;
    }
    else
    {
        Context.SubAsset.reset();
    }
}

class IAssetLoader
{
public:
    virtual ~IAssetLoader() = default;

    virtual AssetType GetAssetType() const = 0;
    virtual const char* GetLoaderName() const = 0;

    virtual void CollectDependencies(const AssetLoadContext&, std::vector<AssetKey>&) const
    {
    }

    virtual std::shared_ptr<const void> Load(const AssetLoadContext& Context, AssetDiagnostic& OutDiagnostic) = 0;
};
