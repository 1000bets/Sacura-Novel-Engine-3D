#include "Core/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/RHI/Renderer.h"

#include <cassert>
#include <chrono>
#include <utility>
#include <future>

RenderThread* RenderThread::Instance = nullptr;

RenderThread::RenderThread() = default;

RenderThread::~RenderThread()
{
    if (GetState() != RenderThreadState::Stopped)
    {
        Stop();
    }
}

RenderThread* RenderThread::Get()
{
    return Instance;
}

void RenderThread::SetState(RenderThreadState NewState)
{
    State.store(NewState, std::memory_order_release);
    ReadyCondition.notify_all();
}

RenderThreadState RenderThread::GetState() const
{
    return State.load(std::memory_order_acquire);
}

void RenderThread::Start(
    const NativeWindowInfo& InWindowInfo,
    const std::string& InShaderDirectory,
    GraphicsBackend InBackendPreference)
{
    assert(GetState() == RenderThreadState::Stopped);

    WindowInfo = InWindowInfo;
    ShaderDirectory = InShaderDirectory;
    BackendPreference = InBackendPreference;
    LastError.clear();
    LastSubmittedFrame.store(0, std::memory_order_release);
    {
        std::lock_guard<std::mutex> Lock(StatisticsMutex);
        PublishedStatistics.clear();
    }

    CommandQueue.ResetForReuse();
    FrameQueue.ResetForReuse();

    Instance = this;
    bStopRequested.store(false, std::memory_order_release);
    SetState(RenderThreadState::Starting);
    Thread = std::thread(&RenderThread::ThreadMain, this);
}

void RenderThread::Stop()
{
    const RenderThreadState Current = GetState();
    if (Current == RenderThreadState::Stopped)
    {
        return;
    }

    SetState(RenderThreadState::Stopping);
    bStopRequested.store(true, std::memory_order_release);
    FrameQueue.RequestShutdown();
    CommandQueue.RequestShutdown();

    if (Thread.joinable())
    {
        Thread.join();
    }

    SetState(RenderThreadState::Stopped);

    if (Instance == this)
    {
        Instance = nullptr;
    }

    PrintString("RenderThread: joined");
}

bool RenderThread::IsReady() const
{
    return GetState() == RenderThreadState::Ready;
}

bool RenderThread::HasFailed() const
{
    return GetState() == RenderThreadState::Failed;
}

const std::string& RenderThread::GetLastError() const
{
    return LastError;
}

bool RenderThread::WaitUntilReady() const
{
    std::unique_lock<std::mutex> Lock(ReadyMutex);
    ReadyCondition.wait(Lock, [this]()
    {
        const RenderThreadState Current = GetState();
        return Current == RenderThreadState::Ready
            || Current == RenderThreadState::Failed
            || Current == RenderThreadState::Stopped;
    });
    return GetState() == RenderThreadState::Ready;
}

RenderCommandEnqueueResult RenderThread::Enqueue(RenderCommandQueue::Command Work)
{
    const RenderCommandEnqueueResult Result = CommandQueue.Enqueue(std::move(Work));
    if (Result == RenderCommandEnqueueResult::Accepted)
    {
        FrameQueue.NotifyWake();
    }
    return Result;
}

bool RenderThread::SubmitFrame(std::unique_ptr<RenderFrameData> Frame)
{
    LastSubmittedFrame.store(Frame->FrameIndex, std::memory_order_release);
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
    const bool bInitialized = OwnedRenderer->Initialize(WindowInfo, BackendPreference, ShaderDirectory);
    if (!bInitialized)
    {
        LastError = "Renderer initialization failed";
        OwnedRenderer.reset();
        {
            std::lock_guard<std::mutex> Lock(ReadyMutex);
            SetState(RenderThreadState::Failed);
        }
        PrintString("RenderThread: FAILED");
        return;
    }

    DefaultMesh = OwnedRenderer->GetDefaultMesh();

    {
        std::lock_guard<std::mutex> Lock(ReadyMutex);
        SetState(RenderThreadState::Ready);
    }
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
    CommandQueue.WaitUntilIdle();

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
        CommandQueue.BeginExecute();
        if (Work)
        {
            Work();
        }
        CommandQueue.EndExecute();
    }
}

void RenderThread::RenderFrame(const RenderFrameData& Frame)
{
    AssertRenderThread();
    SetRenderFrameIndex(Frame.FrameIndex);

    if (OwnedRenderer && OwnedRenderer->IsInitialized())
    {
        OwnedRenderer->Render(Frame);
        std::lock_guard<std::mutex> Lock(StatisticsMutex);
        PublishedStatistics = OwnedRenderer->GetStatistics();
    }
}

RenderStatistics RenderThread::GetStatistics(RenderSurfaceId Surface) const
{
    std::lock_guard<std::mutex> Lock(StatisticsMutex);
    const auto Iterator = PublishedStatistics.find(Surface.Value);
    if (Iterator == PublishedStatistics.end())
    {
        return RenderStatistics{};
    }
    return Iterator->second;
}

bool RenderThread::AttachSurface(RenderSurfaceId Surface, const NativeWindowInfo& NativeWindow)
{
    AssertGameThread();
    if (!IsReady())
    {
        return false;
    }
    auto Completion = std::make_shared<std::promise<bool>>();
    auto Result = Completion->get_future();
    if (Enqueue([this, Surface, NativeWindow, Completion]()
    {
        Completion->set_value(OwnedRenderer->AddSurface(Surface, NativeWindow));
    }) != RenderCommandEnqueueResult::Accepted)
    {
        return false;
    }
    return Result.get();
}

void RenderThread::DetachSurface(RenderSurfaceId Surface)
{
    AssertGameThread();
    if (!IsReady())
    {
        return;
    }
    auto Completion = std::make_shared<std::promise<void>>();
    auto Result = Completion->get_future();
    if (Enqueue([this, Surface, Completion]()
    {
        OwnedRenderer->RemoveSurface(Surface);
        Completion->set_value();
    }) == RenderCommandEnqueueResult::Accepted)
    {
        Result.get();
    }
}

void RenderThread::ResizeSurface(RenderSurfaceId Surface, uint32_t Width, uint32_t Height)
{
    AssertGameThread();
    if (IsReady())
    {
        Enqueue([this, Surface, Width, Height]()
        {
            OwnedRenderer->ResizeSurface(Surface, Width, Height);
        });
    }
}
