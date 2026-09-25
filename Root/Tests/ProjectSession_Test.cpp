#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Project/ProjectDescriptor.h"
#include "Project/ProjectGenerator.h"
#include "Project/ProjectSession.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
int Failures = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++Failures;
        std::cout << "FAIL: " << Message << "\n";
        return;
    }
    std::cout << "PASS: " << Message << "\n";
}

std::filesystem::path MakeTempRoot()
{
    const std::filesystem::path Root =
        std::filesystem::temp_directory_path() / "SakuraProjectSessionTest";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    std::filesystem::create_directories(Root, Error);
    return Root;
}

void WriteTextFile(const std::filesystem::path& Path, const std::string& Contents)
{
    std::filesystem::create_directories(Path.parent_path());
    std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
    Output << Contents;
}
}

int main()
{
    SetCurrentThreadRole(ThreadRole::Game);

    const std::filesystem::path TempRoot = MakeTempRoot();
    const std::filesystem::path ProjectDir = TempRoot / "DemoGame";
    std::filesystem::create_directories(ProjectDir / "Content");
    std::filesystem::create_directories(ProjectDir / "Scripts");

    {
        ProjectDescriptor Discard{};
        std::string Error;
        const std::filesystem::path BadTypeFile = TempRoot / "bad_type.project";
        WriteTextFile(BadTypeFile, R"({"name":42,"engineVersion":"0.1"})");
        Expect(!ProjectDescriptor::TryLoadFromFile(BadTypeFile, Discard, Error), "Reject non-string name without throw");
        Expect(!Error.empty(), "Non-string name reports error");
        Expect(Discard.Name.empty(), "OutDescriptor stays empty on type error");
    }

    {
        ProjectDescriptor Discard{};
        std::string Error;
        const std::filesystem::path EscapeFile = TempRoot / "escape.project";
        WriteTextFile(
            EscapeFile,
            R"({"name":"Escape","engineVersion":"0.1","startupScene":"../Outside.scene"})");
        Expect(!ProjectDescriptor::TryLoadFromFile(EscapeFile, Discard, Error), "Reject startupScene outside project root");
        Expect(Error.find("startupScene") != std::string::npos, "Escape path error mentions startupScene");
    }

    {
        const std::filesystem::path StoryProject = TempRoot / "story.project";
        WriteTextFile(StoryProject,
            R"({"name":"Story","startupStory":"../Outside.story"})");
        ProjectDescriptor Descriptor;
        std::string Error;
        Expect(!ProjectDescriptor::TryLoadFromFile(StoryProject, Descriptor, Error), "Reject startupStory outside project root");
        WriteTextFile(StoryProject,
            R"({"name":"Story","startupStory":"Content/Stories/Opening.story"})");
        Expect(ProjectDescriptor::TryLoadFromFile(StoryProject, Descriptor, Error), "Load configured startupStory");
        Expect(Descriptor.StartupStory == TempRoot / "Content/Stories/Opening.story", "Resolve startupStory relative to project");
        Expect(Descriptor.TrySaveToFile(Error), "Save configured startupStory");
        ProjectDescriptor Reloaded;
        Expect(ProjectDescriptor::TryLoadFromFile(StoryProject, Reloaded, Error), "Reload configured startupStory");
        Expect(Reloaded.StartupStory == Descriptor.StartupStory, "Preserve startupStory on round trip");
    }

    {
        ProjectGeneratorRequest Request{};
        Request.ParentDirectory = TempRoot;
        Request.ProjectName = "..\\Outside";
        ProjectDescriptor Discard{};
        std::string Error;
        Expect(!ProjectGenerator::CreateProject(Request, Discard, Error), "Reject ProjectName path escape");
    }

    {
        const std::filesystem::path GoodFile = ProjectDir / "DemoGame.project";
        WriteTextFile(
            GoodFile,
            R"({"name":"DemoGame","engineVersion":"0.1","template":"Empty","startupScene":"Content/Scenes/Main.scene"})");

        ProjectDescriptor Loaded{};
        std::string Error;
        Expect(ProjectDescriptor::TryLoadFromFile(GoodFile, Loaded, Error), "Load valid project descriptor");
        Expect(Loaded.Name == "DemoGame", "Loaded project name");
        Expect(Loaded.StartupScene.filename() == "Main.scene", "Startup scene stays under project");
    }

    {
        Engine BoundEngine;
        BoundEngine.InitializeHeadless({});

        ProjectSession Session;
        Session.BindEngine(&BoundEngine);

        const std::filesystem::path Missing = TempRoot / "missing.project";
        Expect(!Session.OpenProject(Missing), "Open missing project fails");
        Expect(Session.GetHealth() == ProjectSessionHealth::Failed, "Missing project health is Failed");
        Expect(!Session.GetLastError().empty(), "Missing project exposes LastError");

        const std::filesystem::path GoodFile = ProjectDir / "DemoGame.project";
        Expect(Session.OpenProject(GoodFile), "Open valid project");
        Expect(Session.IsOpen(), "Session is open");
        Expect(
            Session.GetHealth() == ProjectSessionHealth::Ready
                || Session.GetHealth() == ProjectSessionHealth::Degraded,
            "Valid open is Ready or Degraded");
        Expect(Session.GetHealth() == ProjectSessionHealth::Degraded, "Missing startup scene degrades project health");
        Expect(Session.GetIssueCount() == static_cast<int>(BoundEngine.GetAssetRegistry().GetScanDiagnostics().size()) + 1,
            "IssueCount includes the missing startup scene and scan diagnostics");

        BoundEngine.Tick(0.016f);
        BoundEngine.Tick(0.016f);

        Session.CloseProject();
        Expect(!Session.IsOpen(), "Session closes");
        Expect(Session.GetHealth() == ProjectSessionHealth::Closed, "Closed health");

        BoundEngine.Shutdown();
    }

    std::error_code CleanupError;
    std::filesystem::remove_all(TempRoot, CleanupError);

    std::cout << "Failures: " << Failures << "\n";
    return Failures == 0 ? 0 : 1;
}
