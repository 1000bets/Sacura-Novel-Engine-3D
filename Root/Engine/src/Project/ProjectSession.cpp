#include "Project/ProjectSession.h"

#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "World/Scene.h"
#include "World/Serialization/SceneSerializer.h"
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
    Diagnostics.clear();
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
    Diagnostics.clear();
}

bool ProjectSession::RelocateContentPath(
    const std::filesystem::path& Source,
    const std::filesystem::path& Destination,
    std::string& OutError)
{
    if (!OpenedProject.has_value())
    {
        OutError = "No project is open";
        return false;
    }
    ProjectDescriptor Updated = *OpenedProject;
    bool bChanged = false;
    auto Relocate = [&](std::filesystem::path& Path)
    {
        if (Path.empty())
        {
            return;
        }
        const std::filesystem::path Relative = Path.lexically_normal().lexically_relative(Source.lexically_normal());
        if (Relative.empty() || *Relative.begin() == "..")
        {
            return;
        }
        if (Relative == ".")
        {
            Path = Destination.lexically_normal();
        }
        else
        {
            Path = (Destination / Relative).lexically_normal();
        }
        bChanged = true;
    };
    Relocate(Updated.StartupScene);
    Relocate(Updated.StartupStory);
    if (bChanged)
    {
        if (!Updated.TrySaveToFile(OutError))
        {
            return false;
        }
        OpenedProject = std::move(Updated);
        BoundEngine->SetStartupStory(OpenedProject->StartupStory);
    }
    return true;
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
    Diagnostics = BoundEngine->GetProjectDiagnostics();
    IssueCount = static_cast<int>(Diagnostics.size());
    if (!BoundEngine->IsInitialized())
    {
        LastError = LastContentDiagnostic.Message;
        return false;
    }
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
            Diagnostics.push_back(AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "StartupStory", LastError, {}, Descriptor.StartupStory.generic_string()));
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
            Diagnostics.push_back(AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "StartupScene", LastError, {}, Descriptor.StartupScene.generic_string()));
            ++IssueCount;
            PrintString(std::string("ProjectSession: startupScene load failed: ") + LastError);
            return true;
        }

        BoundEngine->AdoptScene(std::unique_ptr<Scene>(LoadedScene));
        PrintString(std::string("ProjectSession: loaded startupScene ") + Descriptor.StartupScene.generic_string());
    }

    return true;
}
