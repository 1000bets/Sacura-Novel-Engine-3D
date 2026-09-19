#include "Project/ProjectSession.h"

#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Project/ProjectPaths.h"

void ProjectSession::BindEngine(Engine* InEngine)
{
    BoundEngine = InEngine;
}

bool ProjectSession::OpenProject(const std::filesystem::path& ProjectFile)
{
    if (BoundEngine == nullptr)
    {
        PrintString("ProjectSession: Engine is not bound");
        return false;
    }

    ProjectDescriptor Descriptor{};
    std::string Error;
    if (!ProjectDescriptor::TryLoadFromFile(ProjectFile, Descriptor, Error))
    {
        PrintString(std::string("ProjectSession: failed to open project: ") + Error);
        return false;
    }

    if (IsOpen())
    {
        CloseProject();
    }

    OpenedProject = Descriptor;
    if (!ApplyOpenedProject())
    {
        OpenedProject.reset();
        ProjectPaths::Clear();
        return false;
    }

    PrintString(std::string("ProjectSession: opened project ") + Descriptor.Name);
    return true;
}

void ProjectSession::CloseProject()
{
    if (!IsOpen())
    {
        return;
    }

    if (BoundEngine != nullptr)
    {
        BoundEngine->UnloadProjectContent();
    }

    ProjectPaths::Clear();
    PrintString("ProjectSession: project closed");
    OpenedProject.reset();
}

bool ProjectSession::IsOpen() const
{
    return OpenedProject.has_value();
}

const ProjectDescriptor* ProjectSession::GetProject() const
{
    if (!OpenedProject.has_value())
    {
        return nullptr;
    }
    return &OpenedProject.value();
}

bool ProjectSession::ApplyOpenedProject()
{
    if (!OpenedProject.has_value() || BoundEngine == nullptr)
    {
        return false;
    }

    const ProjectDescriptor& Descriptor = OpenedProject.value();
    ProjectPaths::SetRoot(Descriptor.ProjectRoot);

    if (!EnginePaths::IsInitialized())
    {
        PrintString("ProjectSession: EnginePaths is not initialized");
        return false;
    }

    BoundEngine->LoadProjectContent(Descriptor);
    return true;
}
