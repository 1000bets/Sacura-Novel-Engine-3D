#pragma once

#include "AssetTools/ImportRequest.h"
#include "AssetTools/ImportResult.h"

#include <filesystem>

class ImageImporter
{
public:
    static ImportResult CopyImage(
        const std::filesystem::path& SourcePath,
        const std::filesystem::path& DestinationPath);
};
