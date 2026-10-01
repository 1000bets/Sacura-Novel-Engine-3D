#pragma once

#include "Assets/ImportResult.h"

#include <filesystem>

class FbxToGlbConverter
{
public:
    static ImportResult Convert(
        const std::filesystem::path& SourceFbxPath,
        const std::filesystem::path& DestinationGlbPath,
        bool bGenerateMissingNormals);
};
