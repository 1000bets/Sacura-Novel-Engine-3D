#include "Core/Threading/RenderCommandQueue.h"

#include <iostream>

namespace
{
int FailureCount = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++FailureCount;
        std::cout << "FAIL: " << Message << std::endl;
    }
    else
    {
        std::cout << "  OK: " << Message << std::endl;
    }
}
}

int main()
{
    std::cout << "RenderLifetime_Test start" << std::endl;

    RenderCommandQueue Queue;

    Expect(Queue.Enqueue({}) == RenderCommandEnqueueResult::RejectedEmpty, "Reject empty command");
    Expect(Queue.Enqueue([]() {}) == RenderCommandEnqueueResult::Accepted, "Accept command");

    RenderCommandQueue::Command Work;
    Expect(Queue.TryDequeue(Work), "Dequeue accepted command");
    Queue.BeginExecute();
    if (Work)
    {
        Work();
    }
    Queue.EndExecute();
    Expect(Queue.GetExecutingCount() == 0, "Executing count cleared");

    Queue.RequestShutdown();
    Expect(Queue.Enqueue([]() {}) == RenderCommandEnqueueResult::RejectedShutdown, "Reject after shutdown");

    Queue.ResetForReuse();
    Expect(Queue.Enqueue([]() {}) == RenderCommandEnqueueResult::Accepted, "Accept after reset");
    Expect(Queue.TryDequeue(Work), "Dequeue after reset");

    std::cout << "Failures: " << FailureCount << std::endl;
    return FailureCount == 0 ? 0 : 1;
}
