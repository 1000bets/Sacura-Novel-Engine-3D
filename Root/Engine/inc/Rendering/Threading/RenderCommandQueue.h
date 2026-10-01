#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>

enum class RenderCommandEnqueueResult : uint8_t
{
    Accepted = 0,
    RejectedShutdown,
    RejectedEmpty
};

class RenderCommandQueue
{
public:
    using Command = std::function<void()>;

    RenderCommandEnqueueResult Enqueue(Command Work);
    bool TryDequeue(Command& OutWork);
    void WaitAndDequeue(Command& OutWork, bool& bShouldExit);
    void RequestShutdown();
    void ResetForReuse();
    void WaitUntilEmpty();
    void WaitUntilIdle();
    bool IsEmpty() const;
    uint32_t GetExecutingCount() const;

    void BeginExecute();
    void EndExecute();

private:
    mutable std::mutex Mutex;
    std::condition_variable Condition;
    std::queue<Command> Commands;
    bool bShutdownRequested = false;
    uint32_t ExecutingCount = 0;
};
