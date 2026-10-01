#pragma once
#include "Assets/AssetRegistry.h"
#include "Materials/MaterialDefinition.h"

class MaterialAssetCreation
{
public:
    static AssetDiagnostic Create(AssetRegistry& Registry, const std::string& VirtualPath,
        const AssetKey& Parent, MaterialDomain Domain, AssetKey& OutAsset);
};
