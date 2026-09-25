#include "Project/ProjectSession.h"

#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Game/Scene.h"
#include "Game/SceneSerializer.h"
#include "Project/ProjectPaths.h"
#include "Story/StoryDocumentIO.h"

void ProjectSession::BindEngine(Engine* InEngine)
{
    BoundEngine = InEngine;
}

bool ProjectSession::OpenProject(const std::filesystem::path& ProjectFile)
{
    LastError.clear();
    IssueCount = 0;
    LastContentDiagnostic = AssetDiagnostic::Ok();
    Health = ProjectSessionHealth::Closed;

    if (BoundEngine == nullptr)
    {
        LastError = "Engine is not bound";
        Health = ProjectSessionHealth::Failed;
        PrintString("ProjectSession: Engine is not bound");
        return false;
    }

    ProjectDescriptor Descriptor{};
    std::string Error;
    if (!ProjectDescriptor::TryLoadFromFile(ProjectFile, Descriptor, Error))
    {
        LastError = Error;
        Health = ProjectSessionHealth::Failed;
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
        Health = ProjectSessionHealth::Failed;
        return false;
    }

    PrintString(std::string("ProjectSession: opened project ") + Descriptor.Name);
    return true;
}

void ProjectSession::CloseProject()
{
    if (!IsOpen())
    {
        Health = ProjectSessionHealth::Closed;
        return;
    }

    if (BoundEngine != nullptr)
    {
        BoundEngine->UnloadProjectContent();
    }

    ProjectPaths::Clear();
    PrintString("ProjectSession: project closed");
    OpenedProject.reset();
    Health = ProjectSessionHealth::Closed;
    IssueCount = 0;
    LastError.clear();
    LastContentDiagnostic = AssetDiagnostic::Ok();
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
        LastError = "Session or Engine is not ready";
        return false;
    }

    const ProjectDescriptor& Descriptor = OpenedProject.value();
    ProjectPaths::SetRoot(Descriptor.ProjectRoot);

    LastContentDiagnostic = BoundEngine->LoadProjectContent(Descriptor);
    IssueCount = static_cast<int>(BoundEngine->GetAssetRegistry().GetScanDiagnostics().size());
    if (LastContentDiagnostic.HasError())
    {
        LastError = LastContentDiagnostic.Message;
        Health = ProjectSessionHealth::Degraded;
        PrintString(std::string("ProjectSession: content scan degraded: ") + LastError);
    }
    else
    {
        Health = ProjectSessionHealth::Ready;
    }

    if (!Descriptor.StartupStory.empty())
    {
        StoryDocument Document;
        const StorySerializeResult Loaded = StoryDocumentIO::LoadFromFile(Descriptor.StartupStory, Document);
        if (!Loaded.bOk)
        {
            LastError = Loaded.Error;
            Health = ProjectSessionHealth::Degraded;
            ++IssueCount;
            PrintString("ProjectSession: startupStory load failed: " + LastError);
        }
    }

    if (!Descriptor.StartupScene.empty())
    {
        Scene* LoadedScene = nullptr;
        const SceneSerializeResult Loaded = SceneSerializer::DeserializeFromFile(Descriptor.StartupScene, LoadedScene);
        if (!Loaded.bOk || LoadedScene == nullptr)
        {
            LastError = Loaded.Error.empty() ? "Failed to load startupScene" : Loaded.Error;
            Health = ProjectSessionHealth::Degraded;
            ++IssueCount;
            PrintString(std::string("ProjectSession: startupScene load failed: ") + LastError);
            return true;
        }

        BoundEngine->AdoptScene(std::unique_ptr<Scene>(LoadedScene));
        PrintString(std::string("ProjectSession: loaded startupScene ") + Descriptor.StartupScene.generic_string());
    }

    return true;
}
