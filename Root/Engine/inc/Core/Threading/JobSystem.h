#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class Job
{
public:
    Job() = default;

    bool IsCompleted() const;
    void Wait() const;

private:
    friend class JobSystem;
    friend class JobGroup;

    struct SharedState
    {
        std::atomic<bool> bCompleted{false};
        mutable std::mutex Mutex;
        mutable std::condition_variable Condition;
    };

    explicit Job(std::shared_ptr<SharedState> State);

    std::shared_ptr<SharedState> State;
};

class JobGroup
{
public:
    JobGroup() = default;

    void Add(std::function<void()> Work);
    void Wait();
    bool IsCompleted() const;

private:
    friend class JobSystem;

    struct SharedState
    {
        std::atomic<int> Remaining{0};
        std::mutex Mutex;
        std::condition_variable Condition;
        bool bAccepting{true};
    };

    std::shared_ptr<SharedState> EnsureState();
    void OnJobFinished();

    std::shared_ptr<SharedState> State;
};

class JobSystem
{
public:
    JobSystem();
    ~JobSystem();

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    void Initialize(unsigned int WorkerCount = 0);
    void Shutdown();

    bool IsInitialized() const { return bInitialized; }
    unsigned int GetWorkerCount() const { return WorkerCount; }

    Job Schedule(std::function<void()> Work);
    void Schedule(JobGroup& Group, std::function<void()> Work);

    static JobSystem* Get();

private:
    struct JobEntry
    {
        std::function<void()> Work;
        std::shared_ptr<Job::SharedState> Completion;
        std::shared_ptr<JobGroup::SharedState> GroupState;
    };

    void WorkerLoop(unsigned int WorkerIndex);
    void Enqueue(JobEntry Entry);
    void CompleteJob(JobEntry& Entry);

    static JobSystem* Instance;

    std::mutex QueueMutex;
    std::condition_variable QueueCondition;
    std::vector<JobEntry> Queue;
    std::vector<std::thread> Workers;

    std::atomic<bool> bAcceptingWork{false};
    std::atomic<bool> bShutdownRequested{false};
    bool bInitialized = false;
    unsigned int WorkerCount = 0;
};
