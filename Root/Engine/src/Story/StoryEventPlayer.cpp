#include "StoryEventPlayer.h"
#include "Story/StoryEvents.h"
#include "Story/StoryRuntime.h"
#include "Authoring/Runtime.h"
#include "Authoring/CoreOperations.h"
#include "Authoring/OperationContext.h"
#include "Core/Object/MemorySubsystem.h"
#include "World/Scene.h"
#include <algorithm>
#include <stdexcept>

namespace
{
// Adapter keeps target lifetime, navigation cancellation and camera blending in
// the existing native action implementation instead of duplicating those rules.
class NativeActionTask final : public Authoring::OperationTask
{
  public:
    NativeActionTask(const Authoring::Arguments& Values, Authoring::OperationContext& Context)
    {
        auto* Memory = MemorySubsystem::Get();
        auto* World = Memory ? Memory->ResolveHandle<Scene>(Context.GetWorld()) : nullptr;
        if (!World)
        {
            throw std::runtime_error("Event world is no longer available");
        }
        StoryActionDefinition Action;
        Action.Kind = static_cast<StoryActionKind>(Values.at("kind").Int64Value);
        Action.DurationSeconds = static_cast<float>(Values.at("seconds").Float64Value);
        Action.TargetObjectId = Values.at("target").StringValue;
        Action.TargetObjectName = Values.at("legacyTarget").StringValue;
        Action.AnimationId = Values.at("animationId").StringValue;
        Action.TargetPosition =
            Vector3(static_cast<float>(Values.at("x").Float64Value), static_cast<float>(Values.at("y").Float64Value),
                    static_cast<float>(Values.at("z").Float64Value));
        Action.TargetRotation = Quaternion(
            static_cast<float>(Values.at("qx").Float64Value), static_cast<float>(Values.at("qy").Float64Value),
            static_cast<float>(Values.at("qz").Float64Value), static_cast<float>(Values.at("qw").Float64Value));
        Action.bHasTargetRotation = Values.at("rotate").BoolValue;
        Action.bUseTargetObjectTransform = Values.at("copy").BoolValue;
        StoryNode Node;
        Node.Id = "action";
        Node.Kind = StoryNodeKind::Action;
        Node.Actions = {Action};
        StoryDocument Document;
        Document.StartNodeId = Node.Id;
        Document.Nodes = {Node};
        Playback.BindScene(World);
        if (!Playback.LoadDocument(Document))
        {
            throw std::runtime_error(Playback.GetLastError());
        }
    }
    Authoring::TaskResult Tick(Authoring::OperationContext&, double Seconds) override
    {
        Playback.Tick(static_cast<float>(Seconds));
        if (Playback.GetPlaybackPhase() == StoryPlaybackPhase::Failed)
        {
            return Authoring::TaskResult::Fail(Playback.GetLastError());
        }
        return Playback.IsFinished() ? Authoring::TaskResult::Complete() : Authoring::TaskResult{};
    }
    void Cancel(Authoring::OperationContext&) noexcept override
    {
        Playback.Stop();
    }

  private:
    StoryRuntime Playback;
};

std::shared_ptr<const Authoring::OperationRegistry> Operations()
{
    static const auto Registry = [] {
        auto Result = std::make_shared<Authoring::OperationRegistry>();
        Authoring::RegisterCoreOperations(*Result);
        Authoring::OperationDefinition Operation;
        Operation.Id = "sacura.story.native-action";
        Operation.Label = "Story action";
        Operation.Contexts = {Authoring::CallSite::Event};
        Operation.Parameters = {{"kind", "Kind", {ReflectedValue::Kind::Int64, 1., 4.}, {}},
                                {"seconds", "Duration", {ReflectedValue::Kind::Float64, 0., {}}, {}},
                                {"target", "Target", {ReflectedValue::Kind::String}, {}},
                                {"legacyTarget", "Legacy target", {ReflectedValue::Kind::String}, {}},
                                {"animationId", "Animation", {ReflectedValue::Kind::String}, {}},
                                {"rotate", "Rotation", {ReflectedValue::Kind::Bool}, {}},
                                {"copy", "Copy target", {ReflectedValue::Kind::Bool}, {}}};
        for (const char* Id : {"x", "y", "z", "qx", "qy", "qz", "qw"})
        {
            Operation.Parameters.push_back({Id, Id, {ReflectedValue::Kind::Float64}, {}});
        }
        const auto Registered = Result->Register(
            std::move(Operation), [](const Authoring::Arguments& Values, Authoring::OperationContext& Context) {
                return std::make_unique<NativeActionTask>(Values, Context);
            });
        if (!Registered.bOk)
        {
            throw std::runtime_error(Registered.Message);
        }
        return Result;
    }();
    return Registry;
}

Authoring::OperationCall Call(const StoryActionDefinition& Action)
{
    Authoring::OperationCall Result;
    Result.Id = Action.Kind == StoryActionKind::Wait ? "sacura.core.wait" : "sacura.story.native-action";
    Result.Parameters["seconds"] = ReflectedValue::MakeFloat64(Action.DurationSeconds);
    if (Action.Kind == StoryActionKind::Wait)
    {
        return Result;
    }
    Result.Parameters["kind"] = ReflectedValue::MakeInt64(static_cast<int>(Action.Kind));
    Result.Parameters["target"] = ReflectedValue::MakeString(Action.TargetObjectId);
    Result.Parameters["legacyTarget"] = ReflectedValue::MakeString(Action.TargetObjectName);
    Result.Parameters["animationId"] = ReflectedValue::MakeString(Action.AnimationId);
    Result.Parameters["rotate"] = ReflectedValue::MakeBool(Action.bHasTargetRotation);
    Result.Parameters["copy"] = ReflectedValue::MakeBool(Action.bUseTargetObjectTransform);
    const double Values[] = {Action.TargetPosition.x, Action.TargetPosition.y, Action.TargetPosition.z,
                             Action.TargetRotation.x, Action.TargetRotation.y, Action.TargetRotation.z,
                             Action.TargetRotation.w};
    size_t Index = 0;
    for (const char* Id : {"x", "y", "z", "qx", "qy", "qz", "qw"})
    {
        Result.Parameters[Id] = ReflectedValue::MakeFloat64(Values[Index++]);
    }
    return Result;
}
} // namespace

struct StoryEventPlayer::Implementation
{
    struct Instance
    {
        StoryEventDefinition Definition;
        StoryEventSnapshot Snapshot;
        std::unique_ptr<Authoring::Runtime> Executor;
        size_t Group = 0;
        std::vector<uint64_t> Runs;
        bool bStarted = false;
        bool bComplete = false;
    };
    ObjectHandle World;
    uint64_t* NextInstanceId = nullptr;
    StoryNodeId NodeId;
    StoryEventPhase Phase = StoryEventPhase::OnStart;
    std::vector<std::shared_ptr<Instance>> Instances, Waiting;
    std::vector<StoryEventSnapshot> History;
    std::vector<StoryEventBatch> Batches;
    StoryDocument Definitions;
    size_t Batch = 0, Binding = 0;
    bool bReady = true;
    std::string Error;

    void StartGroup(Instance& Event)
    {
        Event.Runs.clear();
        if (Event.Group == Event.Definition.Groups.size())
        {
            Event.bComplete = true;
            Event.Snapshot.State = StoryEventPlaybackState::Completed;
            return;
        }
        for (const auto& Action : Event.Definition.Groups[Event.Group].Actions)
        {
            const auto Submission = Event.Executor->Submit(Call(Action));
            if (!Submission.Error.bOk)
            {
                Error = Event.Definition.Name + ": " + Submission.Error.Message;
                return;
            }
            Event.Runs.push_back(Submission.RunId);
        }
    }
    std::shared_ptr<Instance> Start(const StoryEventBinding& Placement)
    {
        if (Instances.size() >= 64)
        {
            Error = "Too many concurrent story events";
            return {};
        }
        const auto* Definition = FindStoryEvent(Definitions, Placement.EventId);
        if (!Definition)
        {
            Error = "Story event no longer exists: " + Placement.EventId;
            return {};
        }
        auto Event = std::make_shared<Instance>();
        Event->Definition = ResolveStoryEvent(*Definition, Placement);
        Event->Snapshot.InstanceId = (*NextInstanceId)++;
        Event->Snapshot.Name = Event->Definition.Name;
        Event->Snapshot.NodeId = NodeId;
        Event->Snapshot.Phase = Phase;
        for (const auto& Group : Event->Definition.Groups)
        {
            for (const auto& Action : Group.Actions)
            {
                // Navigation moves independently in the world tick. It can be
                // cancelled, but suspending its executor alone would not pause it.
                if (Action.Kind == StoryActionKind::NavigateTo || Action.Kind == StoryActionKind::CharacterAnimation)
                {
                    Event->Snapshot.bCanPause = false;
                }
            }
        }
        Event->Executor = std::make_unique<Authoring::Runtime>(Operations(), World);
        const auto Started = Event->Executor->Start();
        if (!Started.bOk)
        {
            Error = Started.Message;
            return {};
        }
        StartGroup(*Event);
        Instances.push_back(Event);
        return Event;
    }
    void Pump()
    {
        if (!Error.empty())
        {
            return;
        }
        // Started joins wait for the first executor tick; finite flow/end joins
        // wait for all groups. NONE releases the phase immediately.
        for (const auto& Event : Waiting)
        {
            if (!Event->bComplete)
            {
                return;
            }
        }
        Waiting.clear();
        while (Batch < Batches.size())
        {
            const auto& Current = Batches[Batch];
            while (Binding < Current.Bindings.size())
            {
                const auto& Placement = Current.Bindings[Binding++];
                auto Event = Start(Placement);
                if (!Event || !Error.empty())
                {
                    return;
                }
                if (Placement.Join != StoryEventJoin::None)
                {
                    if (Placement.Join == StoryEventJoin::Started)
                    {
                        // A separate gate releases on start without ending the instance.
                        StartedWaiting.push_back(Event);
                    }
                    else
                    {
                        Waiting.push_back(Event);
                    }
                }
                if (!Current.bParallel && (!Waiting.empty() || !StartedWaiting.empty()))
                {
                    return;
                }
            }
            if (!Waiting.empty() || !StartedWaiting.empty())
            {
                return;
            }
            ++Batch;
            Binding = 0;
        }
        bReady = true;
    }
    std::vector<std::shared_ptr<Instance>> StartedWaiting;
};

StoryEventPlayer::StoryEventPlayer(ObjectHandle World, uint64_t& NextInstanceId)
    : State(std::make_unique<Implementation>())
{
    State->World = World;
    State->NextInstanceId = &NextInstanceId;
}
StoryEventPlayer::~StoryEventPlayer() = default;

void StoryEventPlayer::BeginPhase(const StoryDocument& Document, const StoryNode& Node, StoryEventPhase Phase)
{
    State->Definitions.Events = Document.Events;
    State->NodeId = Node.Id;
    State->Phase = Phase;
    State->Batches.clear();
    for (const auto& Batch : Node.EventBatches)
    {
        if (Batch.Phase == Phase)
        {
            State->Batches.push_back(Batch);
        }
    }
    State->Batch = State->Binding = 0;
    State->Waiting.clear();
    State->StartedWaiting.clear();
    State->bReady = false;
    State->Pump();
}

bool StoryEventPlayer::Trigger(const StoryDocument& Document, const std::string& EventId)
{
    if (!FindStoryEvent(Document, EventId))
    {
        return false;
    }
    State->Definitions.Events = Document.Events;
    StoryEventBinding Placement;
    Placement.EventId = EventId;
    Placement.Join = StoryEventJoin::None;
    return State->Start(Placement) != nullptr;
}

void StoryEventPlayer::Tick(float DeltaSeconds)
{
    for (auto& Event : State->Instances)
    {
        if (Event->bComplete || Event->Snapshot.State == StoryEventPlaybackState::Paused)
        {
            continue;
        }
        auto Result = Event->Executor->Tick(DeltaSeconds);
        Event->bStarted = true;
        if (!Result.bOk)
        {
            State->Error = Result.Message;
            break;
        }
        bool Complete = true;
        for (uint64_t Run : Event->Runs)
        {
            const auto Snapshot = Event->Executor->GetRun(Run);
            if (!Snapshot || Snapshot->Status == Authoring::RunStatus::Failed ||
                Snapshot->Status == Authoring::RunStatus::Cancelled)
            {
                State->Error =
                    Event->Definition.Name + ": " + (Snapshot ? Snapshot->Error.Message : "Execution expired");
                break;
            }
            Complete &= Snapshot->Status == Authoring::RunStatus::Succeeded;
        }
        if (!State->Error.empty())
        {
            break;
        }
        if (Complete)
        {
            ++Event->Group;
            State->StartGroup(*Event);
        }
    }
    auto& Started = State->StartedWaiting;
    Started.erase(std::remove_if(Started.begin(), Started.end(), [](const auto& Event)
                                { return Event->bStarted || Event->bComplete; }),
                  Started.end());
    if (Started.empty())
    {
        State->Pump();
    }
    auto& Instances = State->Instances;
    for (const auto& Event : Instances)
    {
        if (Event->bComplete)
        {
            State->History.push_back(Event->Snapshot);
        }
    }
    if (State->History.size() > 64)
    {
        State->History.erase(State->History.begin(), State->History.end() - 64);
    }
    Instances.erase(
        std::remove_if(Instances.begin(), Instances.end(), [](const auto& Event) { return Event->bComplete; }),
        Instances.end());
}
bool StoryEventPlayer::IsReady() const
{
    return State->bReady && State->Error.empty();
}
const std::string& StoryEventPlayer::GetError() const
{
    return State->Error;
}

std::vector<StoryEventSnapshot> StoryEventPlayer::GetSnapshots() const
{
    auto Result = State->History;
    for (const auto& Event : State->Instances)
    {
        Result.push_back(Event->Snapshot);
    }
    return Result;
}

bool StoryEventPlayer::SetPaused(uint64_t InstanceId, bool bPaused)
{
    for (auto& Event : State->Instances)
    {
        if (Event->Snapshot.InstanceId == InstanceId && !Event->bComplete && Event->Snapshot.bCanPause)
        {
            Event->Snapshot.State = bPaused ? StoryEventPlaybackState::Paused : StoryEventPlaybackState::Running;
            return true;
        }
    }
    return false;
}

bool StoryEventPlayer::Stop(uint64_t InstanceId)
{
    for (auto& Event : State->Instances)
    {
        if (Event->Snapshot.InstanceId == InstanceId && !Event->bComplete)
        {
            Event->Executor->Stop();
            Event->bComplete = true;
            Event->Snapshot.State = StoryEventPlaybackState::Cancelled;
            // Release phase gates on the next world tick, including paused Step.
            return true;
        }
    }
    return false;
}
