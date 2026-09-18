#include "Core/Threading/RenderCommandQueue.h"

void RenderCommandQueue::Enqueue(Command Work)
{
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        if (bShutdownRequested)
        {
            return;
        }

        Commands.push(std::move(Work));
    }

    Condition.notify_one();
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

void RenderCommandQueue::WaitUntilEmpty()
{
    std::unique_lock<std::mutex> Lock(Mutex);
    Condition.wait(Lock, [this]()
    {
        return Commands.empty();
    });
}

bool RenderCommandQueue::IsEmpty() const
{
    std::lock_guard<std::mutex> Lock(Mutex);
    return Commands.empty();
}
