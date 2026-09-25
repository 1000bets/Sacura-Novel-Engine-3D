#include "Engine.h"
#include "Project/ProjectSession.h"
#include "UI/RenderViewportWidget.h"
#include <QApplication>
#include <QTimer>
#include <QHBoxLayout>
#include <iostream>

int main(int ArgumentCount, char** Arguments)
{
    QApplication Application(ArgumentCount, Arguments);
    Engine Runtime;
    Runtime.SetShaderDirectory(SAKURA_RENDER_SHADER_DIRECTORY);
    if (Application.arguments().contains("--vulkan"))
    {
        Runtime.SetGraphicsBackend(GraphicsBackend::Vulkan);
    }
    Runtime.InitializeHeadless({});
    ProjectSession Session;
    Session.BindEngine(&Runtime);
    if (!Session.OpenProject(SAKURA_RENDER_VALIDATION_PROJECT))
    {
        std::cout << "FAIL: validation project could not be opened\n";
        Runtime.Shutdown();
        return 1;
    }
    QWidget Window;
    QHBoxLayout Layout(&Window);
    RenderViewportWidget SceneViewport(&Window);
    RenderViewportWidget GameViewport(&Window);
    Layout.addWidget(&SceneViewport);
    Layout.addWidget(&GameViewport);
    Window.resize(960, 480);
    Window.show();
    Application.processEvents();
    if (!Runtime.RegisterRenderSurface(RenderSurfaceId{1}, SceneViewport.BuildNativeWindowInfo())
        || !Runtime.RegisterRenderSurface(RenderSurfaceId{2}, GameViewport.BuildNativeWindowInfo()))
    {
        std::cout << "FAIL: two independent surfaces could not be created\n";
        Runtime.StopPresenting();
        Runtime.Shutdown();
        return 1;
    }
    RenderViewCamera Camera;
    Camera.Position = Vector3(0.f, 2.f, 7.f);
    Camera.Target = Vector3(0.f, 1.f, 0.f);
    RenderCamera SceneCamera;
    Camera.BuildRenderCamera(1.f, SceneCamera);
    Runtime.ConfigureRenderSurface(RenderSurfaceId{1}, true, SceneCamera);
    Runtime.ConfigureRenderSurface(RenderSurfaceId{2}, false, RenderCamera{});
    uint32_t FrameCount = 0;
    int Failures = 0;
    auto Expect = [&](bool bCondition, const char* Message)
    {
        if (!bCondition)
        {
            ++Failures;
            std::cout << "FAIL: " << Message << "\n";
        }
    };
    QTimer Timer;
    QObject::connect(&Timer, &QTimer::timeout, &Window, [&]()
    {
        Runtime.Tick(1.f / 60.f);
        ++FrameCount;
        if (FrameCount == 150)
        {
            const auto First = Runtime.GetRenderStatistics(RenderSurfaceId{1});
            const auto Second = Runtime.GetRenderStatistics(RenderSurfaceId{2});
            std::cout << "Resident meshes: " << First.ResidentMeshes << ", textures: " << First.ResidentTextures
                << ", visible: " << First.VisibleObjects << ", culled: " << First.CulledObjects
                << ", shadow views: " << First.ShadowViews << "\n";
            std::cout << "GPU milliseconds: shadows=" << First.ShadowMilliseconds
                << ", opaque=" << First.OpaqueMilliseconds << ", transparency=" << First.TransparencyMilliseconds
                << ", postprocess=" << First.PostprocessMilliseconds << "\n";
            Expect(First.FrameIndex > 30 && Second.FrameIndex > 30, "Both surfaces advance frames");
            Expect(First.ResidentMeshes >= 3 && First.ResidentTextures >= 1, "Validation assets are resident");
            Expect(First.VisibleObjects > 10 && Second.VisibleObjects > 10, "Both cameras draw geometry");
            Expect(First.CulledObjects >= 64 && Second.CulledObjects >= 64, "Offscreen objects are culled");
            Expect(First.ShadowViews == 8 && Second.ShadowViews == 8, "Directional, point and spot shadow views");
            Expect(First.bGpuTimingsAvailable && Second.bGpuTimingsAvailable, "Both surfaces publish GPU duration queries");
            Runtime.ResizeRenderSurface(RenderSurfaceId{2}, 0, 0);
            Runtime.ResizeRenderSurface(RenderSurfaceId{1}, 400, 320);
        }
        if (FrameCount == 180)
        {
            Runtime.ResizeRenderSurface(RenderSurfaceId{2}, 480, 480);
        }
        if (FrameCount == 210)
        {
            Expect(Runtime.GetRenderStatistics(RenderSurfaceId{2}).FrameIndex > 180, "Suspended surface resumes");
            Runtime.UnregisterRenderSurface(RenderSurfaceId{2});
            Expect(Runtime.RegisterRenderSurface(RenderSurfaceId{2}, GameViewport.BuildNativeWindowInfo()), "Surface can be detached and recreated");
        }
        if (FrameCount == 240)
        {
            Session.CloseProject();
        }
        if (FrameCount == 270)
        {
            const auto Statistics = Runtime.GetRenderStatistics(RenderSurfaceId{1});
            Expect(Statistics.ResidentMeshes == 1 && Statistics.ResidentTextures == 0 && Statistics.RetiredResources == 0,
                "Project resources retire after GPU completion while fallback mesh remains");
            Application.quit();
        }
    });
    Timer.start(16);
    QTimer::singleShot(30000, &Application, [&]()
    {
        ++Failures;
        std::cout << "FAIL: GPU validation timeout\n";
        Application.quit();
    });
    Application.exec();
    Timer.stop();
    Runtime.StopPresenting();
    Runtime.Shutdown();
    std::cout << "Failures: " << Failures << "\n";
    if (Failures != 0)
    {
        return 1;
    }
    return 0;
}
