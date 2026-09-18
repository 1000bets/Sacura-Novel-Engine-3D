#pragma once

#include "Core/Threading/RenderFrameData.h"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>

class RenderFrameQueue
{
public:
    explicit RenderFrameQueue(std::size_t MaximumQueuedFrames = 2);

    bool Submit(std::unique_ptr<RenderFrameData> Frame);
    std::unique_ptr<RenderFrameData> WaitForFrame();
    std::unique_ptr<RenderFrameData> WaitForFrameFor(std::chrono::milliseconds Timeout);
    void ReleaseConsumedFrame();
    void RequestShutdown();
    void NotifyWake();
    void WaitUntilEmpty();

    std::size_t GetQueuedFrameCount() const;

private:
    mutable std::mutex Mutex;
    std::condition_variable ProducerCondition;
    std::condition_variable ConsumerCondition;
    std::queue<std::unique_ptr<RenderFrameData>> Frames;
    std::size_t MaximumQueuedFrames = 2;
    bool bShutdownRequested = false;
};
