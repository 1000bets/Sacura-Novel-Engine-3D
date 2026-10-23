#pragma once
#include "Assets/AssetTypes.h"
#include <QImage>
class AssetRegistry;
QImage GenerateAssetPreview(const AssetRegistry& Registry, const AssetKey& Key);
