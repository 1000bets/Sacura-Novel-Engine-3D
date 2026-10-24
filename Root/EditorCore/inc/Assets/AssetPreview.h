#pragma once
#include "Assets/AssetTypes.h"
#include "Core/IO/ImageBuffer.h"
class AssetRegistry;
ImageBuffer GenerateAssetPreview(const AssetRegistry& Registry, const AssetKey& Key);
