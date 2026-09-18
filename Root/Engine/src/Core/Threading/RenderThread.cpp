#include "Core/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/Renderer.h"

#include <cassert>
#include <chrono>
#include <utility>

RenderThread* RenderThread::Instance = nullptr;

RenderThread::RenderThread() = default;

RenderThread::~RenderThread()
{
    if (bRunning.load(std::memory_order_acquire))
    {
        Stop();
    }
}

RenderThread* RenderThread::Get()
{
    return Instance;
}

void RenderThread::Start(
    const NativeWindowInfo& InWindowInfo,
    const std::string& InShaderDirectory,
    GraphicsBackend InBackendPreference)
{
    assert(!bRunning.load(std::memory_order_acquire));

    WindowInfo = InWindowInfo;
    ShaderDirectory = InShaderDirectory;
    BackendPreference = InBackendPreference;

    Instance = this;
    bStopRequested.store(false, std::memory_order_release);
    bReady.store(false, std::memory_order_release);
    bRunning.store(true, std::memory_order_release);
    Thread = std::thread(&RenderThread::ThreadMain, this);
}

void RenderThread::Stop()
{
    if (!bRunning.load(std::memory_order_acquire))
    {
        return;
    }

    bStopRequested.store(true, std::memory_order_release);
    FrameQueue.RequestShutdown();
    CommandQueue.RequestShutdown();

    if (Thread.joinable())
    {
        Thread.join();
    }

    bRunning.store(false, std::memory_order_release);
    bReady.store(false, std::memory_order_release);

    if (Instance == this)
    {
        Instance = nullptr;
    }

    PrintString("RenderThread: joined");
}

bool RenderThread::IsReady() const
{
    return bReady.load(std::memory_order_acquire);
}

void RenderThread::WaitUntilReady() const
{
    std::unique_lock<std::mutex> Lock(ReadyMutex);
    ReadyCondition.wait(Lock, [this]()
    {
        return bReady.load(std::memory_order_acquire);
    });
}

void RenderThread::Enqueue(RenderCommandQueue::Command Work)
{
    CommandQueue.Enqueue(std::move(Work));
    FrameQueue.NotifyWake();
}

bool RenderThread::SubmitFrame(std::unique_ptr<RenderFrameData> Frame)
{
    return FrameQueue.Submit(std::move(Frame));
}

void RenderThread::ResizeRenderer(uint32_t Width, uint32_t Height)
{
    AssertRenderThread();
    if (OwnedRenderer)
    {
        OwnedRenderer->Resize(Width, Height);
    }
}

Renderer* RenderThread::GetRenderer()
{
    AssertRenderThread();
    return OwnedRenderer.get();
}

const Renderer* RenderThread::GetRenderer() const
{
    AssertRenderThread();
    return OwnedRenderer.get();
}

void RenderThread::ThreadMain()
{
    SetCurrentThreadRole(ThreadRole::Render);
    SetCurrentThreadDebugName("Render Thread");
    PrintString("Thread created: Render Thread");

    OwnedRenderer = std::make_unique<Renderer>();
    OwnedRenderer->Initialize(WindowInfo, BackendPreference, ShaderDirectory);
    DefaultMesh = OwnedRenderer->GetDefaultMesh();

    {
        std::lock_guard<std::mutex> Lock(ReadyMutex);
        bReady.store(true, std::memory_order_release);
    }
    ReadyCondition.notify_all();
    PrintString("RenderThread: READY");

    while (!bStopRequested.load(std::memory_order_acquire))
    {
        ProcessPendingCommands();

        std::unique_ptr<RenderFrameData> Frame = FrameQueue.WaitForFrameFor(std::chrono::milliseconds(4));
        if (Frame)
        {
            RenderFrame(*Frame);
            FrameQueue.ReleaseConsumedFrame();
            continue;
        }

        if (bStopRequested.load(std::memory_order_acquire))
        {
            break;
        }
    }

    ProcessPendingCommands();
    CommandQueue.WaitUntilEmpty();

    if (OwnedRenderer)
    {
        OwnedRenderer->Shutdown();
        OwnedRenderer.reset();
    }

    PrintString("RenderThread: exiting");
}

void RenderThread::ProcessPendingCommands()
{
    RenderCommandQueue::Command Work;
    while (CommandQueue.TryDequeue(Work))
    {
        AssertRenderThread();
        if (Work)
        {
            Work();
        }
    }
}

void RenderThread::RenderFrame(const RenderFrameData& Frame)
{
    AssertRenderThread();
    SetRenderFrameIndex(Frame.FrameIndex);

    if (OwnedRenderer && OwnedRenderer->IsInitialized())
    {
        OwnedRenderer->Render(Frame);
    }
}
