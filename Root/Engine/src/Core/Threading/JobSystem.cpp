#include "Core/Threading/JobSystem.h"
#include "Core/Threading/ThreadContext.h"

#include <cassert>
#include <exception>
#include <string>
#include <utility>

JobSystem* JobSystem::Instance = nullptr;

Job::Job(std::shared_ptr<SharedState> InState)
    : State(std::move(InState))
{
}

bool Job::IsCompleted() const
{
    if (!State)
    {
        return true;
    }

    return State->bCompleted.load(std::memory_order_acquire);
}

void Job::Wait() const
{
    if (!State)
    {
        return;
    }

    std::unique_lock<std::mutex> Lock(State->Mutex);
    State->Condition.wait(Lock, [this]()
    {
        return State->bCompleted.load(std::memory_order_acquire);
    });
}

std::shared_ptr<JobGroup::SharedState> JobGroup::EnsureState()
{
    if (!State)
    {
        State = std::make_shared<SharedState>();
    }

    return State;
}

void JobGroup::Add(std::function<void()> Work)
{
    JobSystem* System = JobSystem::Get();
    assert(System && System->IsInitialized());
    System->Schedule(*this, std::move(Work));
}

void JobGroup::Wait()
{
    if (!State)
    {
        return;
    }

    std::unique_lock<std::mutex> Lock(State->Mutex);
    State->bAccepting = false;
    State->Condition.wait(Lock, [this]()
    {
        return State->Remaining.load(std::memory_order_acquire) == 0;
    });
}

bool JobGroup::IsCompleted() const
{
    if (!State)
    {
        return true;
    }

    return !State->bAccepting && State->Remaining.load(std::memory_order_acquire) == 0;
}

void JobGroup::OnJobFinished()
{
    if (!State)
    {
        return;
    }

    const int Remaining = State->Remaining.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (Remaining == 0)
    {
        std::lock_guard<std::mutex> Lock(State->Mutex);
        State->Condition.notify_all();
    }
}

JobSystem::JobSystem() = default;

JobSystem::~JobSystem()
{
    if (bInitialized)
    {
        Shutdown();
    }
}

JobSystem* JobSystem::Get()
{
    return Instance;
}

void JobSystem::Initialize(unsigned int InWorkerCount)
{
    assert(!bInitialized);

    Instance = this;

    const unsigned int HardwareConcurrency = std::thread::hardware_concurrency();
    unsigned int ReservedThreads = 2;
    if (HardwareConcurrency <= ReservedThreads)
    {
        WorkerCount = 1;
    }
    else
    {
        WorkerCount = HardwareConcurrency - ReservedThreads;
    }

    if (InWorkerCount > 0)
    {
        WorkerCount = InWorkerCount;
    }

    bShutdownRequested.store(false, std::memory_order_release);
    bAcceptingWork.store(true, std::memory_order_release);
    bInitialized = true;

    Workers.reserve(WorkerCount);
    for (unsigned int WorkerIndex = 0; WorkerIndex < WorkerCount; ++WorkerIndex)
    {
        Workers.emplace_back(&JobSystem::WorkerLoop, this, WorkerIndex);
    }

    PrintString("JobSystem: started " + std::to_string(WorkerCount) + " worker threads");
}

void JobSystem::Shutdown()
{
    if (!bInitialized)
    {
        return;
    }

    bAcceptingWork.store(false, std::memory_order_release);
    bShutdownRequested.store(true, std::memory_order_release);
    QueueCondition.notify_all();

    for (std::thread& Worker : Workers)
    {
        if (Worker.joinable())
        {
            Worker.join();
        }
    }

    Workers.clear();
    {
        std::lock_guard<std::mutex> Lock(QueueMutex);
        Queue.clear();
    }

    bInitialized = false;
    WorkerCount = 0;

    if (Instance == this)
    {
        Instance = nullptr;
    }

    PrintString("JobSystem: shutdown complete");
}

Job JobSystem::Schedule(std::function<void()> Work)
{
    assert(bInitialized);
    assert(bAcceptingWork.load(std::memory_order_acquire));

    auto Completion = std::make_shared<Job::SharedState>();
    JobEntry Entry;
    Entry.Work = std::move(Work);
    Entry.Completion = Completion;
    Enqueue(std::move(Entry));
    return Job(std::move(Completion));
}

void JobSystem::Schedule(JobGroup& Group, std::function<void()> Work)
{
    assert(bInitialized);
    assert(bAcceptingWork.load(std::memory_order_acquire));

    auto GroupState = Group.EnsureState();
    {
        std::lock_guard<std::mutex> Lock(GroupState->Mutex);
        assert(GroupState->bAccepting);
        GroupState->Remaining.fetch_add(1, std::memory_order_acq_rel);
    }

    JobEntry Entry;
    Entry.Work = std::move(Work);
    Entry.GroupState = GroupState;
    Enqueue(std::move(Entry));
}

void JobSystem::Enqueue(JobEntry Entry)
{
    {
        std::lock_guard<std::mutex> Lock(QueueMutex);
        Queue.push_back(std::move(Entry));
    }

    QueueCondition.notify_one();
}

void JobSystem::CompleteJob(JobEntry& Entry)
{
    if (Entry.Completion)
    {
        {
            std::lock_guard<std::mutex> Lock(Entry.Completion->Mutex);
            Entry.Completion->bCompleted.store(true, std::memory_order_release);
        }
        Entry.Completion->Condition.notify_all();
    }

    if (Entry.GroupState)
    {
        const int Remaining = Entry.GroupState->Remaining.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (Remaining == 0)
        {
            std::lock_guard<std::mutex> Lock(Entry.GroupState->Mutex);
            Entry.GroupState->Condition.notify_all();
        }
    }
}

void JobSystem::WorkerLoop(unsigned int WorkerIndex)
{
    SetCurrentThreadRole(ThreadRole::Worker);
    SetCurrentThreadDebugName("Worker #" + std::to_string(WorkerIndex));
    PrintString("Thread created: Worker #" + std::to_string(WorkerIndex));

    while (true)
    {
        JobEntry Entry;
        {
            std::unique_lock<std::mutex> Lock(QueueMutex);
            QueueCondition.wait(Lock, [this]()
            {
                return bShutdownRequested.load(std::memory_order_acquire) || !Queue.empty();
            });

            if (Queue.empty())
            {
                if (bShutdownRequested.load(std::memory_order_acquire) && !bAcceptingWork.load(std::memory_order_acquire))
                {
                    break;
                }

                continue;
            }

            Entry = std::move(Queue.front());
            Queue.erase(Queue.begin());
        }

        try
        {
            if (Entry.Work)
            {
                Entry.Work();
            }
        }
        catch (const std::exception& Exception)
        {
            PrintString(std::string("JobSystem: job exception: ") + Exception.what());
        }
        catch (...)
        {
            PrintString("JobSystem: job unknown exception");
        }

        CompleteJob(Entry);
    }
}
