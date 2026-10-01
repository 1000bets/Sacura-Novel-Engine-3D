#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Shell/EditorMainWindow.h"
#include "EditorReflectionAnchor.h"
#include "Engine.h"
#include "Project/ProjectSession.h"
#include "Launcher/ProjectBrowserDialog.h"

#include <QApplication>
#include <QTimer>

#include <filesystem>
#include <string>

namespace
{
std::filesystem::path ParseFlagPath(int ArgumentCount, char** Arguments, const char* Flag, const char* FlagEquals)
{
    const std::string FlagName = Flag;
    const std::string FlagPrefix = FlagEquals;
    for (int Index = 1; Index < ArgumentCount; ++Index)
    {
        const std::string Argument = Arguments[Index];
        if (Argument == FlagName && Index + 1 < ArgumentCount)
        {
            return Arguments[Index + 1];
        }
        if (Argument.rfind(FlagPrefix, 0) == 0)
        {
            return Argument.substr(FlagPrefix.size());
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

    const std::filesystem::path LauncherShot = ParseFlagPath(ArgumentCount, Arguments, "--screenshot-launcher", "--screenshot-launcher=");
    if (!LauncherShot.empty())
    {
        ProjectBrowserDialog Browser;
        if (!Browser.CaptureScreenshot(LauncherShot))
        {
            PrintString("SakuraEditor: failed to capture launcher screenshot");
            return 1;
        }
        PrintString(std::string("SakuraEditor: launcher screenshot saved to ") + LauncherShot.generic_string());
        return 0;
    }

    Engine BoundEngine;
    ForceTouchEditorRegistrars();
    const std::string Backend = ParseFlagPath(ArgumentCount, Arguments, "--backend", "--backend=").string();
    if (Backend == "D3D12")
    {
        BoundEngine.SetGraphicsBackend(GraphicsBackend::D3D12);
    }
    else if (Backend == "Vulkan")
    {
        BoundEngine.SetGraphicsBackend(GraphicsBackend::Vulkan);
    }
    else if (Backend == "OpenGL")
    {
        BoundEngine.SetGraphicsBackend(GraphicsBackend::OpenGL);
    }
    else if (!Backend.empty() && Backend != "Auto")
    {
        PrintString("Unknown graphics backend: " + Backend);
        return 1;
    }
    BoundEngine.InitializeHeadless({});
    if (!BoundEngine.IsInitialized())
    {
        PrintString("SakuraEditor: engine initialization failed: " + BoundEngine.GetInitializationError());
        return 1;
    }

    ProjectSession Session;
    Session.BindEngine(&BoundEngine);

    std::filesystem::path ProjectFile = ParseFlagPath(ArgumentCount, Arguments, "--project", "--project=");
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
        PrintString(std::string("SakuraEditor: failed to open project: ") + Session.GetLastError());
        BoundEngine.Shutdown();
        return 1;
    }
    if (Session.GetHealth() == ProjectSessionHealth::Degraded)
    {
        PrintString(std::string("SakuraEditor: project opened in degraded state: ") + Session.GetLastError());
    }

    int ExitCode = 0;
    {
        EditorMainWindow MainWindow(BoundEngine, Session);
        MainWindow.show();

        const std::filesystem::path EditorShot = ParseFlagPath(ArgumentCount, Arguments, "--screenshot-editor", "--screenshot-editor=");
        if (!EditorShot.empty())
        {
            QTimer::singleShot(200, [&]()
            {
                if (!MainWindow.CaptureScreenshot(QString::fromStdString(EditorShot.string())))
                {
                    PrintString("SakuraEditor: failed to capture editor screenshot");
                    Application.exit(1);
                    return;
                }
                PrintString(std::string("SakuraEditor: editor screenshot saved to ") + EditorShot.generic_string());
                Application.exit(0);
            });
        }

        ExitCode = Application.exec();
    }

    Session.CloseProject();
    BoundEngine.StopPresenting();
    BoundEngine.Shutdown();
    return ExitCode;
}
