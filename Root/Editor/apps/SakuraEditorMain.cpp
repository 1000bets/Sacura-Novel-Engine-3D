#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "EditorMainWindow.h"
#include "Engine.h"
#include "Project/ProjectSession.h"
#include "ProjectBrowserDialog.h"

#include <QApplication>

#include <filesystem>
#include <string>

namespace
{
std::filesystem::path ParseProjectArgument(int ArgumentCount, char** Arguments)
{
    for (int Index = 1; Index < ArgumentCount; ++Index)
    {
        const std::string Argument = Arguments[Index];
        if (Argument == "--project" && Index + 1 < ArgumentCount)
        {
            return Arguments[Index + 1];
        }
        if (Argument.rfind("--project=", 0) == 0)
        {
            return Argument.substr(std::string("--project=").size());
        }
    }
    return {};
}

std::filesystem::path ParseScreenshotArgument(int ArgumentCount, char** Arguments)
{
    for (int Index = 1; Index < ArgumentCount; ++Index)
    {
        const std::string Argument = Arguments[Index];
        if (Argument == "--screenshot-launcher" && Index + 1 < ArgumentCount)
        {
            return Arguments[Index + 1];
        }
        if (Argument.rfind("--screenshot-launcher=", 0) == 0)
        {
            return Argument.substr(std::string("--screenshot-launcher=").size());
        }
    }
    return {};
}
}

int main(int ArgumentCount, char** Arguments)
{
    QApplication Application(ArgumentCount, Arguments);
    Q_INIT_RESOURCE(studio);
    Application.setApplicationName("SakuraNovel Studio");
    Application.setOrganizationName("SakuraNovel");

    const std::filesystem::path ExecutablePath = std::filesystem::absolute(Arguments[0]);
    EnginePaths::InitializeFromExecutable(ExecutablePath);

    const std::filesystem::path ScreenshotPath = ParseScreenshotArgument(ArgumentCount, Arguments);
    if (!ScreenshotPath.empty())
    {
        ProjectBrowserDialog Browser;
        if (!Browser.CaptureScreenshot(ScreenshotPath))
        {
            PrintString("SakuraEditor: failed to capture launcher screenshot");
            return 1;
        }
        PrintString(std::string("SakuraEditor: launcher screenshot saved to ") + ScreenshotPath.generic_string());
        return 0;
    }

    Engine BoundEngine;
    BoundEngine.InitializeHeadless({});

    ProjectSession Session;
    Session.BindEngine(&BoundEngine);

    std::filesystem::path ProjectFile = ParseProjectArgument(ArgumentCount, Arguments);
    if (ProjectFile.empty())
    {
        ProjectBrowserDialog Browser;
        if (Browser.exec() != QDialog::Accepted)
        {
            BoundEngine.Shutdown();
            return 0;
        }
        ProjectFile = Browser.SelectedProjectFile();
    }

    if (!Session.OpenProject(ProjectFile))
    {
        PrintString("SakuraEditor: failed to open project");
        BoundEngine.Shutdown();
        return 1;
    }

    EditorMainWindow MainWindow(BoundEngine, Session);
    MainWindow.show();

    const int ExitCode = Application.exec();

    Session.CloseProject();
    BoundEngine.Shutdown();
    return ExitCode;
}
