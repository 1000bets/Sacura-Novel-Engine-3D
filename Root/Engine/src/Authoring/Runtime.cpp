#include "Authoring/Runtime.h"
#include "Authoring/OperationContext.h"
#include "SessionState.h"
#include "Core/Threading/ThreadContext.h"

#include <cmath>
#include <exception>
#include <limits>
#include <utility>

namespace Authoring
{
struct Runtime::Implementation
{
    struct Run
    {
        RunSnapshot Snapshot;
        std::shared_ptr<const RegisteredOperation> Operation;
        Arguments Parameters;
        std::shared_ptr<ExecutionLifetime> Lifetime;
        OperationContext Context;
        std::unique_ptr<OperationTask> Task;
    };

    std::shared_ptr<const OperationRegistry> Registry;
    ObjectHandle World;
    std::shared_ptr<SessionState> Session;
    std::map<uint64_t, std::unique_ptr<Run>> Runs;
    uint64_t NextRunId = 1;
    bool bTicking = false;

    static bool IsActive(const Run& Entry)
    {
        return Entry.Snapshot.Status == RunStatus::Queued || Entry.Snapshot.Status == RunStatus::Running;
    }

    void Finish(Run& Entry, RunStatus Status, Diagnostic Error = {}, ReflectedValue Value = {})
    {
        Entry.Lifetime->bActive = false;
        Entry.Snapshot.Status = Status;
        Entry.Snapshot.Error = std::move(Error);
        Entry.Snapshot.Result = std::move(Value);
        if (Entry.Task)
        {
            const bool bWasTicking = bTicking;
            bTicking = true;
            if (Status != RunStatus::Succeeded)
            {
                Entry.Task->Cancel(Entry.Context);
            }
            Entry.Task.reset();
            bTicking = bWasTicking;
        }
    }

    void TrimHistory()
    {
        for (auto Iterator = Runs.begin(); Runs.size() > 256 && Iterator != Runs.end();)
        {
            if (!IsActive(*Iterator->second))
            {
                Iterator = Runs.erase(Iterator);
            }
            else
            {
                ++Iterator;
            }
        }
    }
};

Runtime::Runtime(std::shared_ptr<const OperationRegistry> Registry, ObjectHandle World)
    : State(std::make_unique<Implementation>())
{
    State->Registry = std::move(Registry);
    State->World = World;
}

Runtime::~Runtime()
{
    Stop();
}

Diagnostic Runtime::Start(const std::vector<VariableDefinition>& Variables, const std::vector<SignalDefinition>& Signals)
{
    AssertGameThread();
    if (State->bTicking || !State->Registry)
    {
        return Diagnostic::Failure("session", "Cannot start without a registry or during a scheduler callback");
    }
    auto Candidate = std::make_shared<SessionState>();
    Candidate->World = State->World;
    if (!IsOwnerAvailable(*Candidate, {}))
    {
        return Diagnostic::Failure("session.world", "World is no longer available");
    }
    for (const auto& Variable : Variables)
    {
        const std::string Path = "variables." + Variable.Id;
        if (Variable.Id.empty() || Variable.Schema.Kind == ReflectedValue::Kind::Empty
            || Candidate->Variables.count(Variable.Id) != 0)
        {
            return Diagnostic::Failure(Path, "Variable needs a unique ID and non-void schema");
        }
        const Diagnostic Valid = ValidateValue(Variable.Schema, Variable.InitialValue, Path);
        if (!Valid.bOk)
        {
            return Valid;
        }
        Candidate->Variables.emplace(Variable.Id, SessionState::Variable{Variable.Schema, Variable.InitialValue});
    }
    for (const auto& Signal : Signals)
    {
        const auto Key = std::make_pair(Signal.Id, Signal.Version);
        const Diagnostic Valid = ValidateSchema(Signal.Payload, "signals." + Signal.Id);
        if (!Valid.bOk)
        {
            return Valid;
        }
        if (!IsNamespacedId(Signal.Id) || Signal.Version == 0 || !Candidate->SignalDefinitions.emplace(Key, Signal).second)
        {
            return Diagnostic::Failure("signals." + Signal.Id, "Signal needs a unique namespaced ID and positive version");
        }
    }
    Stop();
    State->Runs.clear();
    State->Session = std::move(Candidate);
    State->Session->bRunning = true;
    return {};
}

void Runtime::Stop()
{
    AssertGameThread();
    if (State->Session)
    {
        State->Session->bRunning = false;
        State->Session->Signals.clear();
    }
    // Tick owns the task's stack. Cleanup of a Stop invoked there waits until return.
    for (auto& Pair : State->Runs)
    {
        auto& Entry = *Pair.second;
        Entry.Lifetime->bActive = false;
        if (Implementation::IsActive(Entry))
        {
            Entry.Snapshot.Status = RunStatus::Cancelled;
            Entry.Snapshot.Error = Diagnostic::Failure("session", "Execution session stopped");
            if (!State->bTicking)
            {
                State->Finish(Entry, RunStatus::Cancelled, Entry.Snapshot.Error);
            }
        }
    }
}

Submission Runtime::Submit(const OperationCall& Call, ObjectHandle Owner)
{
    AssertGameThread();
    if (!IsRunning() || !IsOwnerAvailable(*State->Session, Owner))
    {
        return {0, Diagnostic::Failure("session", "Session or owner is not active in this world")};
    }
    if (GetActiveCount() >= 64 || State->NextRunId == std::numeric_limits<uint64_t>::max())
    {
        return {0, Diagnostic::Failure("execution", "Active execution or identity limit reached")};
    }
    Arguments Parameters;
    const Diagnostic Prepared = State->Registry->Prepare(Call, Parameters);
    if (!Prepared.bOk)
    {
        return {0, Prepared};
    }
    const uint64_t RunId = State->NextRunId++;
    auto Lifetime = std::make_shared<ExecutionLifetime>();
    Lifetime->Owner = Owner;
    OperationContext Context(State->Session, Lifetime);
    auto Entry = std::make_unique<Implementation::Run>(Implementation::Run{
        {RunId, Call.Id, Call.Version, RunStatus::Queued, {}, {}},
        State->Registry->Find(Call.Id, Call.Version), std::move(Parameters), Lifetime, std::move(Context), {}});
    State->Runs.emplace(RunId, std::move(Entry));
    if (!State->bTicking)
    {
        State->TrimHistory();
    }
    return {RunId, {}};
}

Diagnostic Runtime::Tick(double DeltaSeconds)
{
    AssertGameThread();
    if (State->bTicking || !std::isfinite(DeltaSeconds) || DeltaSeconds < 0.0
        || (State->Session && !std::isfinite(State->Session->GameTime + DeltaSeconds)))
    {
        return Diagnostic::Failure("clock", "Recursive tick, negative or non-finite game delta");
    }
    if (!IsRunning())
    {
        return {};
    }
    State->Session->GameTime += DeltaSeconds;
    State->bTicking = true;
    // std::map keeps entries stable; capture IDs to defer newly submitted work.
    std::vector<uint64_t> Snapshot;
    for (const auto& Pair : State->Runs)
    {
        if (Implementation::IsActive(*Pair.second))
        {
            Snapshot.push_back(Pair.first);
        }
    }
    for (const uint64_t Id : Snapshot)
    {
        auto& Entry = *State->Runs.at(Id);
        if (!Implementation::IsActive(Entry) || !Entry.Context.IsActive())
        {
            State->Finish(Entry, RunStatus::Cancelled, Diagnostic::Failure("owner", "Execution context was revoked"));
            continue;
        }
        try
        {
            if (!Entry.Task)
            {
                Entry.Task = Entry.Operation->CreateTask(Entry.Parameters, Entry.Context);
                Entry.Snapshot.Status = RunStatus::Running;
            }
            if (!Entry.Context.IsActive())
            {
                State->Finish(Entry, RunStatus::Cancelled, Diagnostic::Failure("context", "Execution context was revoked"));
                continue;
            }
            if (!Entry.Task)
            {
                State->Finish(Entry, RunStatus::Failed, Diagnostic::Failure("execution", "Operation factory returned no task"));
                continue;
            }
            TaskResult Result = Entry.Task->Tick(Entry.Context, DeltaSeconds);
            if (!Entry.Context.IsActive())
            {
                State->Finish(Entry, RunStatus::Cancelled, Diagnostic::Failure("context", "Execution context was revoked"));
            }
            else if (Result.Status == RunStatus::Succeeded)
            {
                const Diagnostic Valid = ValidateValue(Entry.Operation->Definition.Result, Result.Value, "result");
                State->Finish(Entry, Valid.bOk ? RunStatus::Succeeded : RunStatus::Failed, Valid, Valid.bOk ? Result.Value : ReflectedValue{});
            }
            else if (Result.Status == RunStatus::Failed)
            {
                State->Finish(Entry, RunStatus::Failed, Result.Error.bOk
                    ? Diagnostic::Failure("execution", "Operation failed without a diagnostic") : Result.Error);
            }
            else if (Result.Status != RunStatus::Running)
            {
                State->Finish(Entry, RunStatus::Failed, Diagnostic::Failure("execution", "Invalid operation task status"));
            }
        }
        catch (const std::exception& Error)
        {
            State->Finish(Entry, Entry.Context.IsActive() ? RunStatus::Failed : RunStatus::Cancelled,
                Diagnostic::Failure("execution", Error.what()));
        }
        catch (...)
        {
            State->Finish(Entry, Entry.Context.IsActive() ? RunStatus::Failed : RunStatus::Cancelled,
                Diagnostic::Failure("execution", "Unknown native operation exception"));
        }
    }
    // Stop/Cancel may have revoked another task during a callback.
    for (auto& Pair : State->Runs)
    {
        auto& Entry = *Pair.second;
        if (Entry.Task && !Entry.Lifetime->bActive)
        {
            State->Finish(Entry, RunStatus::Cancelled, Entry.Snapshot.Error);
        }
    }
    State->bTicking = false;
    State->TrimHistory();
    return {};
}

bool Runtime::Cancel(uint64_t RunId)
{
    AssertGameThread();
    const auto Found = State->Runs.find(RunId);
    if (Found == State->Runs.end() || !Implementation::IsActive(*Found->second))
    {
        return false;
    }
    auto& Entry = *Found->second;
    Entry.Lifetime->bActive = false;
    Entry.Snapshot.Status = RunStatus::Cancelled;
    Entry.Snapshot.Error = Diagnostic::Failure("execution", "Execution cancelled");
    if (!State->bTicking)
    {
        State->Finish(Entry, RunStatus::Cancelled, Entry.Snapshot.Error);
    }
    return true;
}

bool Runtime::IsRunning() const
{
    AssertGameThread();
    return State->Session && State->Session->bRunning;
}

double Runtime::GetGameTime() const
{
    AssertGameThread();
    return State->Session ? State->Session->GameTime : 0.0;
}

size_t Runtime::GetActiveCount() const
{
    AssertGameThread();
    size_t Count = 0;
    for (const auto& Pair : State->Runs)
    {
        Count += Implementation::IsActive(*Pair.second) ? 1 : 0;
    }
    return Count;
}

std::optional<RunSnapshot> Runtime::GetRun(uint64_t RunId) const
{
    AssertGameThread();
    const auto Found = State->Runs.find(RunId);
    return Found == State->Runs.end() ? std::nullopt : std::optional<RunSnapshot>(Found->second->Snapshot);
}

Diagnostic Runtime::ReadVariable(const std::string& Id, ReflectedValue& OutValue) const
{
    AssertGameThread();
    if (State->Session)
    {
        const auto Found = State->Session->Variables.find(Id);
        if (Found != State->Session->Variables.end())
        {
            OutValue = Found->second.Value;
            return {};
        }
    }
    return Diagnostic::Failure("variables." + Id, "Variable is not declared");
}

std::vector<Signal> Runtime::DrainSignals()
{
    AssertGameThread();
    if (!State->Session)
    {
        return {};
    }
    auto& Queue = State->Session->Signals;
    std::vector<Signal> Result(Queue.begin(), Queue.end());
    Queue.clear();
    return Result;
}
}
