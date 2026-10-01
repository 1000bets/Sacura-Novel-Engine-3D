#pragma once

#include "Assets/ImportRequest.h"
#include "Assets/ImportResult.h"

#include <filesystem>

class GlbImporter
{
public:
    static ImportResult ValidateAndCopy(
        const std::filesystem::path& SourcePath,
        const std::filesystem::path& DestinationPath);
};
