#pragma once

#include "Core/Threading/RenderCommandQueue.h"
#include "Core/Threading/RenderFrameData.h"
#include "Core/Threading/RenderFrameQueue.h"
#include "Platform/GraphicsBackend.h"
#include "Platform/NativeWindowInfo.h"
#include "Rendering/RenderResourceHandles.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

class Renderer;

enum class RenderThreadState : uint8_t
{
    Stopped = 0,
    Starting,
    Ready,
    Failed,
    Stopping
};

class RenderThread
{
public:
    RenderThread();
    ~RenderThread();

    RenderThread(const RenderThread&) = delete;
    RenderThread& operator=(const RenderThread&) = delete;

    void Start(
        const NativeWindowInfo& WindowInfo,
        const std::string& ShaderDirectory,
        GraphicsBackend BackendPreference = GraphicsBackend::Auto);
    void Stop();

    RenderThreadState GetState() const;
    bool IsReady() const;
    bool HasFailed() const;
    const std::string& GetLastError() const;
    bool WaitUntilReady() const;

    RenderCommandEnqueueResult Enqueue(RenderCommandQueue::Command Work);
    bool SubmitFrame(std::unique_ptr<RenderFrameData> Frame);
    uint64_t GetRetirementValue() const { return LastSubmittedFrame.load(std::memory_order_acquire) + 1; }
    RenderStatistics GetStatistics(RenderSurfaceId Surface) const;
    void ResizeRenderer(uint32_t Width, uint32_t Height);
    bool AttachSurface(RenderSurfaceId Surface, const NativeWindowInfo& WindowInfo);
    void DetachSurface(RenderSurfaceId Surface);
    void ResizeSurface(RenderSurfaceId Surface, uint32_t Width, uint32_t Height);

    MeshHandle GetDefaultMesh() const { return DefaultMesh; }

    Renderer* GetRenderer();
    const Renderer* GetRenderer() const;

    static RenderThread* Get();

private:
    void ThreadMain();
    void ProcessPendingCommands();
    void RenderFrame(const RenderFrameData& Frame);
    void SetState(RenderThreadState NewState);

    static RenderThread* Instance;

    RenderCommandQueue CommandQueue;
    RenderFrameQueue FrameQueue;

    std::thread Thread;
    std::atomic<RenderThreadState> State{RenderThreadState::Stopped};
    std::atomic<bool> bStopRequested{false};

    mutable std::mutex ReadyMutex;
    mutable std::condition_variable ReadyCondition;
    std::string LastError;

    NativeWindowInfo WindowInfo{};
    std::string ShaderDirectory;
    GraphicsBackend BackendPreference = GraphicsBackend::Auto;
    std::unique_ptr<Renderer> OwnedRenderer;
    MeshHandle DefaultMesh{};
    std::atomic<uint64_t> LastSubmittedFrame{0};
    mutable std::mutex StatisticsMutex;
    std::unordered_map<uint32_t, RenderStatistics> PublishedStatistics;
};
