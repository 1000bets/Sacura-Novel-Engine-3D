#include "Engine.h"
#include "Core/EnginePaths.h"
#include "Core/MemorySubsystem.h"
#include "Core/Threading/RenderFrameData.h"
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

void Engine::LoadProjectContent(const ProjectDescriptor& Descriptor)
{
    AssertGameThread();

    ProjectPaths::SetRoot(Descriptor.ProjectRoot);
    SetContentRoot(ProjectPaths::Content());
    SetScriptsRoot(ProjectPaths::Scripts());

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
}

void Engine::UnloadProjectContent()
{
    AssertGameThread();

    SetActiveScene(nullptr);
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

    if (bCreateWindowAndRender)
    {
        WindowSubsystem* Window = CreateSubsystem<WindowSubsystem>();
        Window->CreateMainWindow("Sacura Novel Engine", 1280, 720, true);

        ResolveShaderDirectory();
        Render.Start(Window->GetNativeWindowInfo(), ShaderDirectory, PreferredBackend);
        Render.WaitUntilReady();
    }

    bRunning = true;
    bInitialized = true;
    NextFrameIndex = 1;
    ActiveScene = nullptr;
}

void Engine::Initialize()
{
    bHeadless = false;
    InitializeCommon(true);
    PrintString("Engine: initialized");
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

void Engine::SetActiveScene(Scene* Scene)
{
    AssertGameThread();
    ActiveScene = Scene;
}

void Engine::Tick(float DeltaTime)
{
    AssertGameThread();
    BeginFrame();

    Assets.PumpCompletions();
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

    if (ActiveScene != nullptr)
    {
        Extractor.Extract(*ActiveScene, Frame->Scene);
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
    ActiveScene = nullptr;

    PrintString("Engine: shutting down");
    Assets.Shutdown();
    Scripting.Shutdown();
    ReflectionSubsystem::Get().Shutdown();

    if (!bHeadless)
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
