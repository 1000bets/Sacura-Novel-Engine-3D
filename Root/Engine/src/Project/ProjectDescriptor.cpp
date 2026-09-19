#include "Project/ProjectDescriptor.h"

#include <fstream>
#include <nlohmann/json.hpp>

bool ProjectDescriptor::TryLoadFromFile(
    const std::filesystem::path& InProjectFile,
    ProjectDescriptor& OutDescriptor,
    std::string& OutError)
{
    std::error_code Error;
    const std::filesystem::path AbsoluteProjectFile = std::filesystem::weakly_canonical(InProjectFile, Error);
    if (Error || !std::filesystem::exists(AbsoluteProjectFile))
    {
        OutError = "Project file not found";
        return false;
    }

    std::ifstream Input(AbsoluteProjectFile);
    if (!Input)
    {
        OutError = "Failed to open project file";
        return false;
    }

    nlohmann::json Document;
    try
    {
        Input >> Document;
    }
    catch (const std::exception& Exception)
    {
        OutError = std::string("Invalid project JSON: ") + Exception.what();
        return false;
    }

    if (!Document.is_object())
    {
        OutError = "Project file root must be a JSON object";
        return false;
    }

    OutDescriptor = ProjectDescriptor{};
    OutDescriptor.ProjectFile = AbsoluteProjectFile;
    OutDescriptor.ProjectRoot = AbsoluteProjectFile.parent_path();
    OutDescriptor.Name = Document.value("name", AbsoluteProjectFile.stem().string());
    OutDescriptor.EngineVersion = Document.value("engineVersion", std::string("0.1"));
    OutDescriptor.TemplateId = Document.value("template", std::string("Empty"));

    if (Document.contains("startupScene") && Document["startupScene"].is_string())
    {
        const std::string StartupRelative = Document["startupScene"].get<std::string>();
        if (!StartupRelative.empty())
        {
            OutDescriptor.StartupScene = (OutDescriptor.ProjectRoot / StartupRelative).lexically_normal();
        }
    }

    return true;
}

bool ProjectDescriptor::TrySaveToFile(std::string& OutError) const
{
    if (ProjectFile.empty())
    {
        OutError = "Project file path is empty";
        return false;
    }

    nlohmann::json Document;
    Document["name"] = Name;
    Document["engineVersion"] = EngineVersion.empty() ? "0.1" : EngineVersion;
    Document["template"] = TemplateId.empty() ? "Empty" : TemplateId;

    if (!StartupScene.empty())
    {
        std::error_code Error;
        std::filesystem::path RelativeStartup = std::filesystem::relative(StartupScene, ProjectRoot, Error);
        if (Error)
        {
            RelativeStartup = StartupScene;
        }
        Document["startupScene"] = RelativeStartup.generic_string();
    }

    std::error_code DirectoryError;
    std::filesystem::create_directories(ProjectFile.parent_path(), DirectoryError);

    std::ofstream Output(ProjectFile);
    if (!Output)
    {
        OutError = "Failed to write project file";
        return false;
    }

    Output << Document.dump(2);
    return true;
}
