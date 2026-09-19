#pragma once

#include <filesystem>
#include <string>

struct ProjectDescriptor
{
    std::string Name;
    std::string EngineVersion;
    std::string TemplateId = "Empty";
    std::filesystem::path ProjectFile;
    std::filesystem::path ProjectRoot;
    std::filesystem::path StartupScene;

    static bool TryLoadFromFile(const std::filesystem::path& ProjectFile, ProjectDescriptor& OutDescriptor, std::string& OutError);
    bool TrySaveToFile(std::string& OutError) const;
};
