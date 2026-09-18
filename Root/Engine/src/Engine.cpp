#include "Engine.h"
#include "Core/MemorySubsystem.h"
#include "Core/Threading/RenderFrameData.h"
#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Platform/WindowSubsystem.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <thread>

Engine::Engine() = default;

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

void Engine::Initialize()
{
    SetCurrentThreadRole(ThreadRole::Game);
    SetCurrentThreadDebugName("Game Thread");
    PrintString("Thread created: Game Thread");

    CreateSubsystem<MemorySubsystem>();
    WindowSubsystem* Window = CreateSubsystem<WindowSubsystem>();
    Window->CreateMainWindow("Sacura Novel Engine", 1280, 720, true);

    Jobs.Initialize();

    if (ShaderDirectory == "shaders")
    {
        std::filesystem::path Candidate = std::filesystem::current_path() / "shaders";
        if (!std::filesystem::exists(Candidate))
        {
            Candidate = std::filesystem::current_path() / "Root" / "Engine" / "shaders";
        }
        if (std::filesystem::exists(Candidate))
        {
            ShaderDirectory = Candidate.string();
        }
    }

    Render.Start(Window->GetNativeWindowInfo(), ShaderDirectory, PreferredBackend);
    Render.WaitUntilReady();

    bRunning = true;
    bInitialized = true;
    NextFrameIndex = 1;
    ActiveScene = nullptr;
    PrintString("Engine: initialized");
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

    TickSubsystems(DeltaTime);

    if (WindowSubsystem* Window = GetWindowSubsystem())
    {
        if (Window->IsCloseRequested())
        {
            RequestShutdown();
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
    Render.Stop();
    Jobs.Shutdown();
    ShutdownSubsystems();

    bInitialized = false;
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
