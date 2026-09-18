#pragma once

#include "AssetTools/ImportRequest.h"
#include "AssetTools/ImportResult.h"

#include <filesystem>

class GlbImporter
{
public:
    static ImportResult ValidateAndCopy(
        const std::filesystem::path& SourcePath,
        const std::filesystem::path& DestinationPath);
};
