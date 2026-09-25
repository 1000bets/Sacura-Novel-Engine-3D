#include "Core/Threading/RenderFrameQueue.h"

RenderFrameQueue::RenderFrameQueue(std::size_t InMaximumQueuedFrames)
    : MaximumQueuedFrames(InMaximumQueuedFrames == 0 ? 1 : InMaximumQueuedFrames)
{
}

bool RenderFrameQueue::Submit(std::unique_ptr<RenderFrameData> Frame)
{
    std::unique_lock<std::mutex> Lock(Mutex);
    ProducerCondition.wait(Lock, [this]()
    {
        return bShutdownRequested || Frames.size() < MaximumQueuedFrames;
    });

    if (bShutdownRequested)
    {
        return false;
    }

    Frames.push(std::move(Frame));
    ConsumerCondition.notify_one();
    return true;
}

std::unique_ptr<RenderFrameData> RenderFrameQueue::WaitForFrame()
{
    std::unique_lock<std::mutex> Lock(Mutex);
    ConsumerCondition.wait(Lock, [this]()
    {
        return bShutdownRequested || !Frames.empty();
    });

    if (Frames.empty())
    {
        return nullptr;
    }

    std::unique_ptr<RenderFrameData> Frame = std::move(Frames.front());
    Frames.pop();
    ProducerCondition.notify_one();
    return Frame;
}

std::unique_ptr<RenderFrameData> RenderFrameQueue::WaitForFrameFor(std::chrono::milliseconds Timeout)
{
    std::unique_lock<std::mutex> Lock(Mutex);
    ConsumerCondition.wait_for(Lock, Timeout, [this]()
    {
        return bShutdownRequested || !Frames.empty();
    });

    if (Frames.empty())
    {
        return nullptr;
    }

    std::unique_ptr<RenderFrameData> Frame = std::move(Frames.front());
    Frames.pop();
    ProducerCondition.notify_one();
    return Frame;
}

void RenderFrameQueue::ReleaseConsumedFrame()
{
}

void RenderFrameQueue::RequestShutdown()
{
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        bShutdownRequested = true;
    }

    ProducerCondition.notify_all();
    ConsumerCondition.notify_all();
}

void RenderFrameQueue::ResetForReuse()
{
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        bShutdownRequested = false;
        while (!Frames.empty())
        {
            Frames.pop();
        }
    }

    ProducerCondition.notify_all();
    ConsumerCondition.notify_all();
}

void RenderFrameQueue::NotifyWake()
{
    ConsumerCondition.notify_one();
}

void RenderFrameQueue::WaitUntilEmpty()
{
    std::unique_lock<std::mutex> Lock(Mutex);
    ProducerCondition.wait(Lock, [this]()
    {
        return Frames.empty();
    });
}

std::size_t RenderFrameQueue::GetQueuedFrameCount() const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    return Frames.size();
}
