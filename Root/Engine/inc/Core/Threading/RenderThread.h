#pragma once

#include "Core/Threading/RenderCommandQueue.h"
#include "Core/Threading/RenderFrameData.h"
#include "Core/Threading/RenderFrameQueue.h"
#include "Platform/GraphicsBackend.h"
#include "Platform/NativeWindowInfo.h"
#include "Rendering/RenderResourceHandles.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

class Renderer;

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

    bool IsReady() const;
    void WaitUntilReady() const;

    void Enqueue(RenderCommandQueue::Command Work);
    bool SubmitFrame(std::unique_ptr<RenderFrameData> Frame);
    void ResizeRenderer(uint32_t Width, uint32_t Height);

    MeshHandle GetDefaultMesh() const { return DefaultMesh; }

    Renderer* GetRenderer();
    const Renderer* GetRenderer() const;

    static RenderThread* Get();

private:
    void ThreadMain();
    void ProcessPendingCommands();
    void RenderFrame(const RenderFrameData& Frame);

    static RenderThread* Instance;

    RenderCommandQueue CommandQueue;
    RenderFrameQueue FrameQueue;

    std::thread Thread;
    std::atomic<bool> bRunning{false};
    std::atomic<bool> bReady{false};
    std::atomic<bool> bStopRequested{false};

    mutable std::mutex ReadyMutex;
    mutable std::condition_variable ReadyCondition;

    NativeWindowInfo WindowInfo{};
    std::string ShaderDirectory;
    GraphicsBackend BackendPreference = GraphicsBackend::Auto;
    std::unique_ptr<Renderer> OwnedRenderer;
    MeshHandle DefaultMesh{};
};
