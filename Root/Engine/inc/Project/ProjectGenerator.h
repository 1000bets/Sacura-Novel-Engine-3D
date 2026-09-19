#pragma once

#include "Project/ProjectDescriptor.h"

#include <filesystem>
#include <string>

struct ProjectGeneratorRequest
{
    std::filesystem::path ParentDirectory;
    std::string ProjectName;
    std::string EngineVersion = "0.1";
    std::string StartupScene = "Content/Scenes/Main.scene";
    std::string TemplateId = "Empty";
};

class ProjectGenerator
{
public:
    static bool CreateProject(const ProjectGeneratorRequest& Request, ProjectDescriptor& OutDescriptor, std::string& OutError);
};
