#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

class RenderCommandQueue
{
public:
    using Command = std::function<void()>;

    void Enqueue(Command Work);
    bool TryDequeue(Command& OutWork);
    void WaitAndDequeue(Command& OutWork, bool& bShouldExit);
    void RequestShutdown();
    void WaitUntilEmpty();
    bool IsEmpty() const;

private:
    mutable std::mutex Mutex;
    std::condition_variable Condition;
    std::queue<Command> Commands;
    bool bShutdownRequested = false;
};
