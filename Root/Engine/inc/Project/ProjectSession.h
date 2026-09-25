#pragma once

#include "Project/ProjectDescriptor.h"
#include "Assets/AssetTypes.h"

#include <filesystem>
#include <optional>
#include <string>

class Engine;

enum class ProjectSessionHealth
{
    Closed = 0,
    Ready,
    Degraded,
    Failed
};

class ProjectSession
{
public:
    void BindEngine(Engine* InEngine);

    bool OpenProject(const std::filesystem::path& ProjectFile);
    void CloseProject();

    bool IsOpen() const;
    const ProjectDescriptor* GetProject() const;

    ProjectSessionHealth GetHealth() const { return Health; }
    const std::string& GetLastError() const { return LastError; }
    int GetIssueCount() const { return IssueCount; }
    const AssetDiagnostic& GetLastContentDiagnostic() const { return LastContentDiagnostic; }

private:
    bool ApplyOpenedProject();

    Engine* BoundEngine = nullptr;
    std::optional<ProjectDescriptor> OpenedProject;
    ProjectSessionHealth Health = ProjectSessionHealth::Closed;
    std::string LastError;
    int IssueCount = 0;
    AssetDiagnostic LastContentDiagnostic = AssetDiagnostic::Ok();
};
