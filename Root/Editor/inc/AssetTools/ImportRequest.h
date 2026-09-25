#pragma once

#include <filesystem>
#include <string>

struct ImportRequest
{
    std::filesystem::path SourcePath;
    std::string DestinationRelativePath;
    std::filesystem::path StagingDirectory;
    bool bOverwrite = false;
    bool bGenerateMissingNormals = true;
};
