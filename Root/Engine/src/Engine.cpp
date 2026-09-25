#include "Engine.h"
#include "Core/EnginePaths.h"
#include "Core/MemorySubsystem.h"
#include "Core/Threading/RenderFrameData.h"
#include "Core/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Platform/WindowSubsystem.h"
#include "Project/ProjectPaths.h"
#include "Reflection/ReflectionSubsystem.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

Engine::Engine()
    : Scripting(ScriptingSubsystem::Get())
{
}

Engine::~Engine()
{
    if (bInitialized)
    {
        Shutdown();
    }
}

WindowSubsystem* Engine::GetWindowSubsystem() const
{
    return const_cast<Engine*>(this)->GetSubsystem<WindowSubsystem>();
}

void Engine::SetContentRoot(const std::filesystem::path& InContentRoot)
{
    ContentRoot = InContentRoot;
    Registry.SetGameContentRoot(ContentRoot);
}

void Engine::SetScriptsRoot(const std::filesystem::path& InScriptsRoot)
{
    ScriptsRoot = InScriptsRoot;
}

void Engine::ResolveShaderDirectory()
{
    if (!ShaderDirectory.empty())
    {
        return;
    }

    if (EnginePaths::IsInitialized())
    {
        const std::filesystem::path ShadersPath = EnginePaths::Shaders();
        if (std::filesystem::exists(ShadersPath))
        {
            ShaderDirectory = ShadersPath.string();
            return;
        }
    }

    std::filesystem::path Candidate = std::filesystem::current_path() / "shaders";
    if (!std::filesystem::exists(Candidate))
    {
        Candidate = std::filesystem::current_path() / "Engine" / "Shaders";
    }
    if (std::filesystem::exists(Candidate))
    {
        ShaderDirectory = Candidate.string();
    }
}

void Engine::ScanConfiguredContent()
{
    if (EnginePaths::IsInitialized())
    {
        Registry.SetEngineContentRoot(EnginePaths::Content());
    }

    if (ContentRoot.empty() && Registry.GetEngineContentRoot().empty())
    {
        return;
    }

    Registry.SetGameContentRoot(ContentRoot);
    const AssetDiagnostic ScanDiagnostic = Registry.ScanContent();
    if (ScanDiagnostic.HasError())
    {
        PrintString(std::string("AssetRegistry scan failed: ") + ScanDiagnostic.Message);
    }
}

void Engine::ImportProjectScripts()
{
    Scripting.SetScriptsRoot(ScriptsRoot);
    if (!Scripting.IsPythonEnabled())
    {
        return;
    }

    const ReflectionDiagnostic Imported = Scripting.ImportConfiguredModules();
    if (!Imported.bOk)
    {
        PrintString(std::string("Scripting import failed: ") + Imported.Message);
    }
}

AssetDiagnostic Engine::LoadProjectContent(const ProjectDescriptor& Descriptor)
{
    AssertGameThread();

    ProjectPaths::SetRoot(Descriptor.ProjectRoot);
    SetContentRoot(ProjectPaths::Content());
    SetScriptsRoot(ProjectPaths::Scripts());
    StartupStory = Descriptor.StartupStory;

    if (EnginePaths::IsInitialized())
    {
        Registry.SetEngineContentRoot(EnginePaths::Content());
    }

    Registry.SetGameContentRoot(ContentRoot);
    const AssetDiagnostic ScanDiagnostic = Registry.ScanContent();
    if (ScanDiagnostic.HasError())
    {
        PrintString(std::string("AssetRegistry project scan failed: ") + ScanDiagnostic.Message);
    }

    if (bInitialized)
    {
        ImportProjectScripts();
    }

    return ScanDiagnostic;
}

void Engine::UnloadProjectContent()
{
    AssertGameThread();

    AdoptScene({});
    StartupStory.clear();
    GpuUploader.Clear();
    Assets.Shutdown();

    ContentRoot.clear();
    ScriptsRoot.clear();
    Registry.SetGameContentRoot({});
    if (EnginePaths::IsInitialized())
    {
        Registry.SetEngineContentRoot(EnginePaths::Content());
        Registry.ScanContent();
    }
    else
    {
        Registry.Clear();
    }
    ProjectPaths::Clear();
    if (bInitialized)
    {
        Assets.Initialize(Registry, Jobs);
    }
}

void Engine::InitializeCommon(bool bCreateWindowAndRender)
{
    SetCurrentThreadRole(ThreadRole::Game);
    SetCurrentThreadDebugName("Game Thread");
    PrintString("Thread created: Game Thread");

    CreateSubsystem<MemorySubsystem>();

    Jobs.Initialize();

    {
        const ReflectionDiagnostic ReflectionInit = ReflectionSubsystem::Get().InitializeNative();
        if (!ReflectionInit.bOk)
        {
            PrintString(std::string("Reflection InitializeNative failed: ") + ReflectionInit.Message);
        }
    }

    {
        Scripting.SetScriptsRoot(ScriptsRoot);
        const ReflectionDiagnostic ScriptingInit = Scripting.Initialize();
        if (!ScriptingInit.bOk)
        {
            PrintString(std::string("Scripting initialize failed: ") + ScriptingInit.Message);
        }
        else
        {
            ImportProjectScripts();
        }
    }

    ScanConfiguredContent();
    Assets.Initialize(Registry, Jobs);
    AssetResolver.Bind(&Registry, &Assets, &GpuUploader);

    bInitialized = true;
    Play.BindEngine(this);

    if (bCreateWindowAndRender)
    {
        WindowSubsystem* Window = CreateSubsystem<WindowSubsystem>();
        Window->CreateMainWindow("Sacura Novel Engine", 1280, 720, true);

        ResolveShaderDirectory();
        const NativeWindowInfo WindowInfo = Window->GetNativeWindowInfo();
        PresentationWidth = WindowInfo.Width;
        PresentationHeight = WindowInfo.Height;
        Render.Start(WindowInfo, ShaderDirectory, PreferredBackend);
        if (!Render.WaitUntilReady())
        {
            PrintString(std::string("Engine: render thread failed: ") + Render.GetLastError());
            Render.Stop();
            bRunning = false;
            Shutdown();
            return;
        }
    }

    bRunning = true;
    NextFrameIndex = 1;
}

void Engine::Initialize()
{
    bHeadless = false;
    InitializeCommon(true);
    if (bInitialized)
    {
        PrintString("Engine: initialized");
    }
}

void Engine::InitializeHeadless(const std::filesystem::path& InContentRoot)
{
    bHeadless = true;
    SetCurrentThreadRole(ThreadRole::Game);
    SetCurrentThreadDebugName("Game Thread");
    SetContentRoot(InContentRoot);
    if (ScriptsRoot.empty() && !InContentRoot.empty())
    {
        const std::filesystem::path SiblingScripts = InContentRoot.parent_path() / "Scripts";
        if (std::filesystem::exists(SiblingScripts))
        {
            ScriptsRoot = SiblingScripts;
        }
        else
        {
            ScriptsRoot = InContentRoot / "Scripts";
        }
    }
    InitializeCommon(false);
    PrintString("Engine: initialized headless");
}

bool Engine::StartPresenting(const NativeWindowInfo& WindowInfo)
{
    AssertGameThread();
    if (!bInitialized)
    {
        PrintString("Engine: StartPresenting requires Initialize/InitializeHeadless first");
        return false;
    }

    if (Render.GetState() == RenderThreadState::Ready)
    {
        return true;
    }
    if (Render.GetState() == RenderThreadState::Starting)
    {
        return Render.WaitUntilReady();
    }
    if (Render.GetState() != RenderThreadState::Stopped)
    {
        PrintString("Engine: StartPresenting rejected — RenderThread is not stopped");
        return false;
    }

    if (WindowInfo.WindowHandle == nullptr || WindowInfo.Width == 0 || WindowInfo.Height == 0)
    {
        PrintString("Engine: StartPresenting rejected — invalid native window");
        return false;
    }

    ResolveShaderDirectory();
    Render.Start(WindowInfo, ShaderDirectory, PreferredBackend);
    if (!Render.WaitUntilReady())
    {
        PrintString(std::string("Engine: StartPresenting failed: ") + Render.GetLastError());
        return false;
    }

    PresentationWidth = WindowInfo.Width;
    PresentationHeight = WindowInfo.Height;
    bHeadless = false;
    Presentations[1].Window = WindowInfo;
    PrintString("Engine: presenting to editor native surface");
    return true;
}

bool Engine::IsPresenting() const
{
    return Render.IsReady();
}

void Engine::StopPresenting()
{
    AssertGameThread();
    if (Render.GetState() == RenderThreadState::Stopped)
    {
        return;
    }
    Render.Stop();
    Presentations.clear();
    GpuUploader.Clear();
    bHeadless = true;
    PrintString("Engine: presentation stopped");
}

void Engine::AdoptScene(std::unique_ptr<Scene> NewScene)
{
    AssertGameThread();
    StopGame();
    Play.StopPlay();
    OwnedScene = std::move(NewScene);
}

Scene* Engine::GetActiveScene() const
{
    if (Play.IsSimulating())
    {
        return Play.GetPlayWorld();
    }
    return OwnedScene.get();
}

void Engine::BeginStory(Scene* World)
{
    Story.BindScene(World);
    if (!StartupStory.empty())
    {
        Story.LoadFromFile(StartupStory);
    }
}

bool Engine::StartGame()
{
    AssertGameThread();
    if (!bInitialized || OwnedScene == nullptr || Play.IsSimulating())
    {
        return false;
    }
    bGameRunning = true;
    BeginStory(OwnedScene.get());
    return true;
}

void Engine::StopGame()
{
    bGameRunning = false;
    Story.BindScene(nullptr);
}

void Engine::TickWorld(Scene& World, float DeltaTime)
{
    AssertGameThread();
    World.Tick(DeltaTime);
    Story.Tick(DeltaTime);
}

void Engine::SetEditorRenderCamera(const RenderCamera& Camera)
{
    AssertGameThread();
    bEditorRenderCamera = true;
    EditorRenderCamera = Camera;
    ConfigureRenderSurface(RenderSurfaceId{1}, true, Camera);
}

void Engine::UseGameRenderCamera()
{
    AssertGameThread();
    bEditorRenderCamera = false;
    ConfigureRenderSurface(RenderSurfaceId{1}, false, RenderCamera{});
}

void Engine::ResizePresentation(uint32_t Width, uint32_t Height)
{
    AssertGameThread();
    PresentationWidth = Width;
    PresentationHeight = Height;
    ResizeRenderSurface(RenderSurfaceId{1}, Width, Height);
}

bool Engine::RegisterRenderSurface(RenderSurfaceId Surface, const NativeWindowInfo& WindowInfo)
{
    AssertGameThread();
    if (!Surface.IsValid() || WindowInfo.WindowHandle == nullptr || WindowInfo.Width == 0 || WindowInfo.Height == 0)
    {
        return false;
    }
    if (!Render.IsReady())
    {
        if (Surface.Value != 1 || !StartPresenting(WindowInfo))
        {
            return false;
        }
        return true;
    }
    auto Existing = Presentations.find(Surface.Value);
    if (Existing != Presentations.end())
    {
        if (Existing->second.Window.WindowHandle == WindowInfo.WindowHandle)
        {
            return true;
        }
        UnregisterRenderSurface(Surface);
    }
    if (!Render.AttachSurface(Surface, WindowInfo))
    {
        return false;
    }
    Presentations[Surface.Value].Window = WindowInfo;
    return true;
}

void Engine::UnregisterRenderSurface(RenderSurfaceId Surface)
{
    AssertGameThread();
    if (Presentations.erase(Surface.Value) != 0)
    {
        Render.DetachSurface(Surface);
    }
}

void Engine::ResizeRenderSurface(RenderSurfaceId Surface, uint32_t Width, uint32_t Height)
{
    AssertGameThread();
    auto Existing = Presentations.find(Surface.Value);
    if (Existing != Presentations.end())
    {
        Existing->second.Window.Width = Width;
        Existing->second.Window.Height = Height;
    }
    Render.ResizeSurface(Surface, Width, Height);
}

void Engine::ConfigureRenderSurface(RenderSurfaceId Surface, bool bEditScene, const RenderCamera& Camera, const RenderSettings& Settings)
{
    AssertGameThread();
    auto Existing = Presentations.find(Surface.Value);
    if (Existing != Presentations.end())
    {
        Existing->second.bEditScene = bEditScene;
        Existing->second.Camera = Camera;
        Existing->second.Settings = Settings;
    }
}

void Engine::SetRenderSurfaceImGuiOverlay(RenderSurfaceId Surface, ImGuiOverlaySnapshot Overlay)
{
    AssertGameThread();
    auto Existing = Presentations.find(Surface.Value);
    if (Existing != Presentations.end())
    {
        Existing->second.Overlay = std::move(Overlay);
    }
}

void Engine::Tick(float DeltaTime)
{
    AssertGameThread();
    BeginFrame();

    Assets.PumpCompletions();
    Scene* ActiveScene = GetActiveScene();
    if (ActiveScene != nullptr)
    {
        AssetResolver.ResolveScene(*ActiveScene);
    }
    if (OwnedScene != nullptr && OwnedScene.get() != ActiveScene)
    {
        AssetResolver.ResolveScene(*OwnedScene);
    }
    if (Play.IsSimulating())
    {
        Play.Tick(DeltaTime);
    }
    else if (bGameRunning && OwnedScene != nullptr)
    {
        TickWorld(*OwnedScene, DeltaTime);
    }
    TickSubsystems(DeltaTime);

    if (!bHeadless)
    {
        if (WindowSubsystem* Window = GetWindowSubsystem())
        {
            if (Window->IsCloseRequested())
            {
                RequestShutdown();
            }
        }
    }

    EndFrame();
}

void Engine::BeginFrame()
{
    SetGameFrameIndex(NextFrameIndex);
}

void Engine::EndFrame()
{
    if (bHeadless)
    {
        ++NextFrameIndex;
        return;
    }

    auto Frame = std::make_unique<RenderFrameData>();
    Frame->FrameIndex = NextFrameIndex;

    if (Presentations.empty())
    {
        Scene* SceneToRender = GetActiveScene();
        if (bEditorRenderCamera)
        {
            SceneToRender = OwnedScene.get();
        }
        if (SceneToRender != nullptr)
        {
            Extractor.Extract(*SceneToRender, Frame->Scene);
        }
        if (bEditorRenderCamera)
        {
            Frame->Scene.Camera = EditorRenderCamera;
        }
        else if (Frame->Scene.Camera.bValid && PresentationHeight > 0)
        {
            RenderCamera& Camera = Frame->Scene.Camera;
            Camera.AspectRatio = static_cast<float>(PresentationWidth) / static_cast<float>(PresentationHeight);
            Camera.Projection = Matrix::CreatePerspectiveFieldOfView(
                Camera.FieldOfView * (3.14159265f / 180.f), Camera.AspectRatio, Camera.NearPlane, Camera.FarPlane);
            Camera.ViewProjection = Camera.View * Camera.Projection;
        }
    }

    for (auto& Entry : Presentations)
    {
        PresentationState& Presentation = Entry.second;
        if (Presentation.Window.Width == 0 || Presentation.Window.Height == 0)
        {
            continue;
        }
        RenderViewFrame View;
        View.Surface = RenderSurfaceId{Entry.first};
        View.Settings = Presentation.Settings;
        Scene* World = GetActiveScene();
        if (Presentation.bEditScene)
        {
            World = OwnedScene.get();
        }
        if (World != nullptr)
        {
            Extractor.Extract(*World, View.Scene);
        }
        if (Presentation.Camera.bValid)
        {
            View.Scene.Camera = Presentation.Camera;
        }
        RenderCamera& Camera = View.Scene.Camera;
        if (Camera.bValid)
        {
            Camera.AspectRatio = static_cast<float>(Presentation.Window.Width) / static_cast<float>(Presentation.Window.Height);
            Camera.Projection = Matrix::CreatePerspectiveFieldOfView(
                Camera.FieldOfView * 0.0174532925f, Camera.AspectRatio, Camera.NearPlane, Camera.FarPlane);
            Camera.ViewProjection = Camera.View * Camera.Projection;
        }
        View.Overlay = std::move(Presentation.Overlay);
        Frame->Views.push_back(std::move(View));
    }
    Render.SubmitFrame(std::move(Frame));
    ++NextFrameIndex;
}

void Engine::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    AssertGameThread();
    bRunning = false;
    StopGame();
    Play.StopPlay();
    OwnedScene.reset();

    PrintString("Engine: shutting down");
    GpuUploader.Clear();
    Assets.Shutdown();
    Scripting.Shutdown();
    ReflectionSubsystem::Get().Shutdown();

    if (!bHeadless || Render.GetState() != RenderThreadState::Stopped)
    {
        Render.Stop();
    }

    Jobs.Shutdown();
    ShutdownSubsystems();

    bInitialized = false;
    bHeadless = false;
    PrintString("Engine: shutdown complete");
}

void Engine::Run()
{
    Initialize();

    while (bRunning)
    {
        constexpr float TempDelta = 1.f / 60.f;
        Tick(TempDelta);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    Shutdown();
}

void Engine::RequestShutdown()
{
    bRunning = false;
}
