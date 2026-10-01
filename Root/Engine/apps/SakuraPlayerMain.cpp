#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Project/ProjectSession.h"
#include "UI/RenderViewportWidget.h"
#include "UI/StoryWidget.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <filesystem>
#include <string>

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

int main(int ArgumentCount, char** Arguments)
{
    QApplication Application(ArgumentCount, Arguments);
    Application.setApplicationName("SakuraPlayer");
    Application.setOrganizationName("SakuraNovel");
    SetCurrentThreadRole(ThreadRole::Game);
    EnginePaths::InitializeFromExecutable(std::filesystem::absolute(Arguments[0]));
    const std::filesystem::path ProjectFile = ParseFlagPath(ArgumentCount, Arguments, "--project", "--project=");
    if (ProjectFile.empty())
    {
        PrintString("SakuraPlayer: missing --project <path.project>");
        return 1;
    }

    Engine EngineInstance;
    const std::string Backend = ParseFlagPath(ArgumentCount, Arguments, "--backend", "--backend=").string();
    if (Backend == "D3D12")
    {
        EngineInstance.SetGraphicsBackend(GraphicsBackend::D3D12);
    }
    else if (Backend == "Vulkan")
    {
        EngineInstance.SetGraphicsBackend(GraphicsBackend::Vulkan);
    }
    else if (Backend == "OpenGL")
    {
        EngineInstance.SetGraphicsBackend(GraphicsBackend::OpenGL);
    }
    else if (!Backend.empty() && Backend != "Auto")
    {
        PrintString("Unknown graphics backend: " + Backend);
        return 1;
    }
    EngineInstance.InitializeHeadless({});
    if (!EngineInstance.IsInitialized())
    {
        PrintString("SakuraPlayer: engine initialization failed: " + EngineInstance.GetInitializationError());
        return 1;
    }
    ProjectSession Session;
    Session.BindEngine(&EngineInstance);
    if (!Session.OpenProject(ProjectFile) || EngineInstance.GetEditScene() == nullptr)
    {
        QString Error = QString::fromStdString(Session.GetLastError());
        if (Error.isEmpty())
        {
            Error = "Project has no playable startup scene.";
        }
        QMessageBox::critical(nullptr, "SakuraPlayer", Error);
        EngineInstance.Shutdown();
        return 1;
    }

    QWidget Window;
    Window.setWindowTitle(QString::fromStdString(Session.GetProject()->Name));
    Window.resize(1280, 800);
    auto* Layout = new QVBoxLayout(&Window);
    Layout->setContentsMargins(0, 0, 0, 0);
    auto* Viewport = new RenderViewportWidget(&Window);
    auto* Dialogue = new StoryWidget(EngineInstance.GetStoryRuntime(), &Window);
    Layout->addWidget(Viewport, 1);
    Layout->addWidget(Dialogue);
    QObject::connect(Viewport, &RenderViewportWidget::ViewportResized, &Window,
        [&](RenderViewportWidget* Surface)
        {
            if (EngineInstance.IsPresenting())
            {
                const NativeWindowInfo Info = Surface->BuildNativeWindowInfo();
                EngineInstance.ResizePresentation(Info.Width, Info.Height);
            }
        });
    QObject::connect(Viewport, &RenderViewportWidget::NativeSurfaceChanged, &Window,
        [&](RenderViewportWidget* Surface)
        {
            if (!Surface->HasValidNativeHandle())
            {
                EngineInstance.UnregisterRenderSurface(RenderSurfaceId{1});
            }
            else
            {
                EngineInstance.RegisterRenderSurface(RenderSurfaceId{1}, Surface->BuildNativeWindowInfo());
            }
        });
    Window.show();
    if (!EngineInstance.StartPresenting(Viewport->BuildNativeWindowInfo()))
    {
        QMessageBox::critical(&Window, "SakuraPlayer", "Failed to initialize the renderer.");
        Session.CloseProject();
        EngineInstance.Shutdown();
        return 1;
    }
    EngineInstance.UseGameRenderCamera();
    QElapsedTimer Clock;
    Clock.start();
    bool bGameStarted = false;
    const auto Screenshot = ParseFlagPath(ArgumentCount, Arguments, "--screenshot-player", "--screenshot-player=");
    int PresentedFrames = 0;
    QTimer Timer;
    QObject::connect(&Timer, &QTimer::timeout, &Window, [&]()
    {
        const float DeltaTime = std::min(0.25f, static_cast<float>(Clock.restart()) / 1000.0f);
        EngineInstance.Tick(DeltaTime);
        if (!bGameStarted && EngineInstance.PrepareSceneMaterials(*EngineInstance.GetEditScene()))
        {
            bGameStarted = EngineInstance.StartGame();
        }
        Dialogue->Refresh(true);
        if (bGameStarted && !Screenshot.empty())
        {
            ++PresentedFrames;
            if (PresentedFrames >= 30)
            {
                if (!Window.grab().save(QString::fromStdString(Screenshot.string())))
                {
                    PrintString("SakuraPlayer: failed to save screenshot");
                    Application.exit(1);
                    return;
                }
                PrintString("SakuraPlayer: screenshot saved to " + Screenshot.string());
                Application.quit();
            }
        }
        if (!EngineInstance.IsRunning())
        {
            Application.quit();
        }
    });
    Timer.start(16);
    const int ExitCode = Application.exec();
    Timer.stop();
    Session.CloseProject();
    EngineInstance.StopPresenting();
    EngineInstance.Shutdown();
    return ExitCode;
}
