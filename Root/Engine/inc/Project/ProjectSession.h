#pragma once

#include "Project/ProjectDescriptor.h"

#include <filesystem>
#include <optional>
#include <string>

class Engine;

class ProjectSession
{
public:
    void BindEngine(Engine* InEngine);

    bool OpenProject(const std::filesystem::path& ProjectFile);
    void CloseProject();

    bool IsOpen() const;
    const ProjectDescriptor* GetProject() const;

private:
    bool ApplyOpenedProject();

    Engine* BoundEngine = nullptr;
    std::optional<ProjectDescriptor> OpenedProject;
};
