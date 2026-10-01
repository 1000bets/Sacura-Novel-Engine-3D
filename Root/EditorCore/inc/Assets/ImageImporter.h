#pragma once

#include "Assets/ImportRequest.h"
#include "Assets/ImportResult.h"

#include <filesystem>

class ImageImporter
{
public:
    static ImportResult CopyImage(
        const std::filesystem::path& SourcePath,
        const std::filesystem::path& DestinationPath);
};
