#include "Core/Threading/RenderCommandQueue.h"

RenderCommandEnqueueResult RenderCommandQueue::Enqueue(Command Work)
{
    if (!Work)
    {
        return RenderCommandEnqueueResult::RejectedEmpty;
    }

    {
        std::lock_guard<std::mutex> Lock(Mutex);
        if (bShutdownRequested)
        {
            return RenderCommandEnqueueResult::RejectedShutdown;
        }

        Commands.push(std::move(Work));
    }

    Condition.notify_one();
    return RenderCommandEnqueueResult::Accepted;
}

bool RenderCommandQueue::TryDequeue(Command& OutWork)
{
    std::lock_guard<std::mutex> Lock(Mutex);
    if (Commands.empty())
    {
        return false;
    }

    OutWork = std::move(Commands.front());
    Commands.pop();
    Condition.notify_all();
    return true;
}

void RenderCommandQueue::WaitAndDequeue(Command& OutWork, bool& bShouldExit)
{
    std::unique_lock<std::mutex> Lock(Mutex);
    Condition.wait(Lock, [this]()
    {
        return bShutdownRequested || !Commands.empty();
    });

    if (!Commands.empty())
    {
        OutWork = std::move(Commands.front());
        Commands.pop();
        bShouldExit = false;
        Condition.notify_all();
        return;
    }

    OutWork = nullptr;
    bShouldExit = bShutdownRequested;
}

void RenderCommandQueue::RequestShutdown()
{
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        bShutdownRequested = true;
    }

    Condition.notify_all();
}

void RenderCommandQueue::ResetForReuse()
{
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        bShutdownRequested = false;
        while (!Commands.empty())
        {
            Commands.pop();
        }
        ExecutingCount = 0;
    }
    Condition.notify_all();
}

void RenderCommandQueue::WaitUntilEmpty()
{
    std::unique_lock<std::mutex> Lock(Mutex);
    Condition.wait(Lock, [this]()
    {
        return Commands.empty();
    });
}

void RenderCommandQueue::WaitUntilIdle()
{
    std::unique_lock<std::mutex> Lock(Mutex);
    Condition.wait(Lock, [this]()
    {
        return Commands.empty() && ExecutingCount == 0;
    });
}

bool RenderCommandQueue::IsEmpty() const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    return Commands.empty();
}

uint32_t RenderCommandQueue::GetExecutingCount() const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    return ExecutingCount;
}

void RenderCommandQueue::BeginExecute()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    ++ExecutingCount;
}

void RenderCommandQueue::EndExecute()
{
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        if (ExecutingCount > 0)
        {
            --ExecutingCount;
        }
    }
    Condition.notify_all();
}
