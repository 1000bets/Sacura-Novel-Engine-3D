#pragma once
#include "Assets/AssetRegistry.h"

class StoryAssetCreation
{
  public:
    static AssetDiagnostic Create(AssetRegistry& Registry, const std::string& VirtualPath, AssetKey& Created);
};
