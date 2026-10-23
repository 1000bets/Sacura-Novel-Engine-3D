#pragma once

#include <filesystem>
#include <string>

class GameExporter
{
public:
    static bool Export(const std::filesystem::path& ProjectFile,
        const std::filesystem::path& RuntimeDirectory,
        const std::filesystem::path& EngineRoot,
        const std::filesystem::path& Destination,
        std::string& OutError);
};
