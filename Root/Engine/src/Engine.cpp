#include "World/Components/MeshRendererComponent.h"
#include "World/Components/CameraComponent.h"
#include "World/GameObject.h"
#include "Engine.h"
#include "Core/EnginePaths.h"
#include "Core/Object/MemorySubsystem.h"
#include "Rendering/Frame/RenderFrameData.h"
#include "Rendering/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"
#include "World/Scene.h"
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

ReflectionDiagnostic Engine::ImportProjectScripts()
{
    Scripting.SetScriptsRoot(ScriptsRoot);
    if (!Scripting.IsPythonEnabled())
    {
        return ReflectionDiagnostic::Ok();
    }

    if (!Scripting.IsInitialized())
    {
        return ReflectionDiagnostic::Fail("Scripting initialization failed: " + InitializationError);
    }

    const ReflectionDiagnostic Imported = Scripting.ImportConfiguredModules();
    if (!Imported.bOk)
    {
        PrintString(std::string("Scripting import failed: ") + Imported.Message);
    }
    return Imported;
}

AssetDiagnostic Engine::LoadProjectContent(const ProjectDescriptor& Descriptor)
{
    AssertGameThread();
    ProjectDiagnostics.clear();
    if (!bInitialized)
    {
        const AssetDiagnostic Failure = AssetDiagnostic::Fail(AssetErrorCode::InternalError, "ProjectLoad", "Engine initialization failed: " + InitializationError);
        ProjectDiagnostics.push_back(Failure);
        return Failure;
    }

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

    ProjectDiagnostics = Registry.GetScanDiagnostics();
    if (ScanDiagnostic.HasError() && ProjectDiagnostics.empty())
    {
        ProjectDiagnostics.push_back(ScanDiagnostic);
    }
    const ReflectionDiagnostic Imported = ImportProjectScripts();
    for (const ReflectionDiagnostic& Diagnostic : Scripting.GetImportDiagnostics())
    {
        ProjectDiagnostics.push_back(AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "ScriptImport", Diagnostic.Message));
    }
    if (!Imported.bOk && Scripting.GetImportDiagnostics().empty())
    {
        ProjectDiagnostics.push_back(AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "ScriptImport", Imported.Message));
    }
    if (!ProjectDiagnostics.empty())
    {
        return ProjectDiagnostics.front();
    }
    return AssetDiagnostic::Ok();
}

void Engine::UnloadProjectContent()
{
    AssertGameThread();

    AdoptScene({});
    StartupStory.clear();
    AssetResolver.Clear();
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
    bInitialized = true;
    InitializationError.clear();

    {
        const ReflectionDiagnostic ReflectionInit = ReflectionSubsystem::Get().InitializeNative();
        if (!ReflectionInit.bOk)
        {
            InitializationError = ReflectionInit.Message;
            PrintString(std::string("Reflection InitializeNative failed: ") + InitializationError);
            Shutdown();
            return;
        }
    }

    {
        Scripting.SetScriptsRoot(ScriptsRoot);
        Scripting.BindMaterialEngine(this);
        const ReflectionDiagnostic ScriptingInit = Scripting.Initialize();
        if (!ScriptingInit.bOk)
        {
            InitializationError = ScriptingInit.Message;
            PrintString(std::string("Scripting initialize failed: ") + InitializationError);
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
        Presentation.Register(RenderSurfaceId{1}, WindowInfo);
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
    if (bInitialized)
    {
        PrintString("Engine: initialized headless");
    }
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
        PrintString("Engine: StartPresenting rejected вЂ” RenderThread is not stopped");
        return false;
    }

    if (WindowInfo.WindowHandle == nullptr || WindowInfo.Width == 0 || WindowInfo.Height == 0)
    {
        PrintString("Engine: StartPresenting rejected вЂ” invalid native window");
        return false;
    }

    ResolveShaderDirectory();
    Render.Start(WindowInfo, ShaderDirectory, PreferredBackend);
    if (!Render.WaitUntilReady())
    {
        PrintString(std::string("Engine: StartPresenting failed: ") + Render.GetLastError());
        return false;
    }

    bHeadless = false;
    Presentation.Register(RenderSurfaceId{1}, WindowInfo);
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
    Presentation.Clear();
    AssetResolver.Clear();
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
    if (!PrepareSceneMaterials(*OwnedScene))
    {
        return false;
    }
    MaterialTime = 0.f;
    bGameRunning = true;
    OwnedScene->BeginPlay();
    BeginStory(OwnedScene.get());
    return true;
}

std::shared_ptr<DynamicMaterialInstance> Engine::CreateDynamicMaterialInstance(const AssetKey& Material)
{
    AssertGameThread();
    const auto Snapshot = AssetResolver.ResolveMaterial(Material);
    if (!Snapshot)
    {
        return {};
    }
    return std::make_shared<DynamicMaterialInstance>(*Snapshot->Material);
}

void Engine::StopGame()
{
    Story.BindScene(nullptr);
    if (bGameRunning && OwnedScene != nullptr)
    {
        OwnedScene->EndPlay();
    }
    bGameRunning = false;
}

void Engine::TickWorld(Scene& World, float DeltaTime)
{
    AssertGameThread();
    MaterialTime += std::max(0.f, DeltaTime);
    World.Tick(DeltaTime);
    Story.Tick(DeltaTime);
}

void Engine::SetEditorRenderCamera(const RenderCamera& Camera)
{
    AssertGameThread();
    ConfigureRenderSurface(RenderSurfaceId{1}, true, Camera);
}

void Engine::UseGameRenderCamera()
{
    AssertGameThread();
    ConfigureRenderSurface(RenderSurfaceId{1}, false, RenderCamera{});
}

void Engine::ResizePresentation(uint32_t Width, uint32_t Height)
{
    AssertGameThread();
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
    if (Presentation.Contains(Surface, WindowInfo))
    {
        return true;
    }
    UnregisterRenderSurface(Surface);
    if (!Render.AttachSurface(Surface, WindowInfo))
    {
        return false;
    }
    Presentation.Register(Surface, WindowInfo);
    return true;
}

void Engine::UnregisterRenderSurface(RenderSurfaceId Surface)
{
    AssertGameThread();
    if (Presentation.Unregister(Surface))
    {
        Render.DetachSurface(Surface);
    }
}

void Engine::ResizeRenderSurface(RenderSurfaceId Surface, uint32_t Width, uint32_t Height)
{
    AssertGameThread();
    Presentation.Resize(Surface, Width, Height);
    Render.ResizeSurface(Surface, Width, Height);
}

void Engine::ConfigureRenderSurface(RenderSurfaceId Surface, bool bEditScene, const RenderCamera& Camera, const RenderSettings& Settings)
{
    AssertGameThread();
    Presentation.Configure(Surface, bEditScene, Camera, Settings);
}

void Engine::SetRenderSurfaceImGuiOverlay(RenderSurfaceId Surface, ImGuiOverlaySnapshot Overlay)
{
    AssertGameThread();
    Presentation.SetOverlay(Surface, std::move(Overlay));
}

void Engine::Tick(float DeltaTime)
{
    AssertGameThread();
    BeginFrame();

    AssetResolver.PollMaterialChanges();
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
    if (!Play.IsSimulating() && !bGameRunning)
    {
        MaterialTime += std::max(0.f, DeltaTime);
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

    Render.SubmitFrame(Presentation.BuildFrame(NextFrameIndex, GetActiveScene(), OwnedScene.get(), MaterialTime));
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
    AssetResolver.Clear();
    GpuUploader.Clear();
    Assets.Shutdown();
    Scripting.Shutdown();
    Scripting.BindMaterialEngine(nullptr);
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

bool Engine::PrepareSceneMaterials(Scene& World)
{
    AssertGameThread();
    if (!Render.IsReady())
    {
        return true;
    }
    bool bReady = true;
    auto Prepare = [&](const AssetKey& Material, const std::shared_ptr<DynamicMaterialInstance>& Dynamic)
    {
        if (!Material.IsValid() && !Dynamic)
        {
            return;
        }
        const auto Snapshot = AssetResolver.ResolveMaterial(Material, Dynamic);
        if (!Snapshot)
        {
            bReady = bReady && !AssetResolver.GetMaterialDiagnostic(Material).empty();
            return;
        }
        const auto Compilation = GpuUploader.RequestMaterial(*Snapshot->Material);
        const auto State = Compilation->State.load();
        bReady = bReady && State != MaterialCompilationState::Queued && State != MaterialCompilationState::Compiling;
    };
    for (auto Iterator = MaterialWidgetBindings.begin(); Iterator != MaterialWidgetBindings.end();)
    {
        const auto Binding = Iterator->lock();
        if (!Binding)
        {
            Iterator = MaterialWidgetBindings.erase(Iterator);
            continue;
        }
        if (Binding->bVisible && !Binding->bPreview)
        {
            Prepare(Binding->Asset, Binding->Dynamic);
        }
        ++Iterator;
    }
    for (auto* Object : World.GetAllObjects())
    {
        for (auto* Component : Object->GetAllComponents())
        {
            if (auto* Mesh = dynamic_cast<MeshRendererComponent*>(Component))
            {
                Prepare(Mesh->GetMaterialAssetKey(), {});
                Prepare({}, Mesh->DynamicMaterial);
                for (const auto& Slot : Mesh->MaterialSlots.Entries)
                {
                    Prepare(Slot.Material, {});
                }
                for (const auto& Slot : Mesh->DynamicMaterialSlots)
                {
                    Prepare({}, Slot.second);
                }
            }
            if (auto* Camera = dynamic_cast<CameraComponent*>(Component))
            {
                for (const auto& Effect : Camera->PostProcessEffects.Entries)
                {
                    if (Effect.bEnabled && Effect.Intensity > 0.f)
                    {
                        Prepare(Effect.Material, {});
                        Prepare({}, Effect.DynamicMaterial);
                    }
                }
            }
        }
    }
    return bReady;
}

void Engine::RegisterMaterialWidgetBinding(const std::shared_ptr<MaterialWidgetBinding>& Binding)
{
    AssertGameThread();
    MaterialWidgetBindings.push_back(Binding);
}
