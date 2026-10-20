#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Project/ProjectSession.h"
#include "UI/RenderViewportWidget.h"
#include <QApplication>
#include <QTimer>
#include <iostream>

int main(int ArgumentCount, char** Arguments)
{
    QApplication Application(ArgumentCount, Arguments);
    if (ArgumentCount != 2)
    {
        return 1;
    }
    SetCurrentThreadRole(ThreadRole::Game);
    EnginePaths::InitializeFromExecutable(std::filesystem::absolute(Arguments[0]));
    Engine Instance;
    Instance.SetGraphicsBackend(GraphicsBackend::Vulkan);
    Instance.InitializeHeadless({});
    ProjectSession Session;
    Session.BindEngine(&Instance);
    if (!Instance.IsInitialized() || !Session.OpenProject(Arguments[1]))
    {
        return 2;
    }
    int ExitCode = 1;
    {
        RenderViewportWidget Viewport;
        Viewport.resize(640, 480);
        Viewport.show();
        if (!Instance.StartPresenting(Viewport.BuildNativeWindowInfo()))
        {
            return 3;
        }
        // Frame the sample meshes explicitly; coverage must not depend on its
        // authored camera composition (which currently looks above the cubes).
        RenderViewCamera ViewCamera;
        ViewCamera.Position = Vector3(-2.2f, 2.0f, 6.0f);
        ViewCamera.Target = Vector3(-2.2f, 0.0f, 1.4f);
        RenderCamera Camera;
        ViewCamera.BuildRenderCamera(640.0f / 480.0f, Camera);
        Instance.ConfigureRenderSurface(RenderSurfaceId{1}, false, Camera);
        int Frames = 0;
        QTimer Timer;
        QObject::connect(&Timer, &QTimer::timeout, &Application, [&]()
        {
            Instance.Tick(1.0f / 60.0f);
            if (!Instance.IsGameRunning() && Instance.PrepareSceneMaterials(*Instance.GetEditScene()))
            {
                Instance.StartGame();
            }
            const RenderStatistics Statistics = Instance.GetRenderStatistics(RenderSurfaceId{1});
            if (++Frames >= 80 && Instance.IsGameRunning() && Statistics.FrameIndex >= 30
                && Statistics.VisibleObjects > 0 && Statistics.DrawCalls > 0 && Statistics.Triangles > 0
                && Statistics.ResidentMeshes > 0)
            {
                std::cout << "Mac Vulkan render PASS: frame=" << Statistics.FrameIndex
                    << ", visible=" << Statistics.VisibleObjects << ", draws=" << Statistics.DrawCalls
                    << ", triangles=" << Statistics.Triangles << ", meshes=" << Statistics.ResidentMeshes << '\n';
                Application.exit(0);
            }
        });
        QTimer::singleShot(30000, &Application, [&]()
        {
            const auto Statistics = Instance.GetRenderStatistics(RenderSurfaceId{1});
            std::cerr << "Mac Vulkan render timeout: frame=" << Statistics.FrameIndex
                << ", draws=" << Statistics.DrawCalls << ", visible=" << Statistics.VisibleObjects << '\n';
            Application.exit(4);
        });
        Timer.start(16);
        ExitCode = Application.exec();
        Timer.stop();
        Instance.StopPresenting();
    }
    Session.CloseProject();
    Instance.Shutdown();
    return ExitCode;
}
