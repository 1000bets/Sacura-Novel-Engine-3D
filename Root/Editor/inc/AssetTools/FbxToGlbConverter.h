#pragma once

#include "AssetTools/ImportResult.h"

#include <filesystem>

class FbxToGlbConverter
{
public:
    static ImportResult Convert(
        const std::filesystem::path& SourceFbxPath,
        const std::filesystem::path& DestinationGlbPath);
};
