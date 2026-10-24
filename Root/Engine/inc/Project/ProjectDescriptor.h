#pragma once

#include <filesystem>
#include <string>
#include <map>
#include <optional>
#include "Input/InputTypes.h"

struct ProjectDescriptor
{
    std::string Name;
    std::string EngineVersion;
    std::string TemplateId = "Empty";
    std::filesystem::path ProjectFile;
    std::filesystem::path ProjectRoot;
    std::filesystem::path StartupScene;
    std::filesystem::path StartupStory;
    std::filesystem::path StartupAudio;
    std::map<std::string, std::string> NarrativeWidgets;
    std::map<std::string, std::string> NarrativeInputActions;
    std::optional<std::vector<InputContextActivation>> StartupInputContexts;

    static bool TryLoadFromFile(const std::filesystem::path& ProjectFile, ProjectDescriptor& OutDescriptor, std::string& OutError);
    bool TrySaveToFile(std::string& OutError) const;
};
