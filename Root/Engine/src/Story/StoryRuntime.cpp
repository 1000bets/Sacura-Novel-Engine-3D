#include "World/Components/CharacterComponent.h"
#include "Story/StoryRuntime.h"
#include "Characters/CharacterDefinition.h"
#include "Assets/AssetManager.h"
#include "World/Components/NavigationAgentComponent.h"

#include "Core/Threading/ThreadContext.h"
#include "Core/Object/MemorySubsystem.h"
#include "World/Scene.h"
#include "World/Components/CameraComponent.h"
#include "World/GameObject.h"
#include "Story/StoryDocumentIO.h"
#include "Story/StoryEvents.h"
#include "StoryEventPlayer.h"

#include <algorithm>
#include <cmath>

std::shared_ptr<const CharacterDefinition> StoryRuntime::GetSpeakerCharacter() const
{
    if (DisplayedSpeaker.rfind("character:", 0) != 0 || !GetScene() || !GetScene()->GetAssetManager())
    {
        return nullptr;
    }
    AssetKey Key;
    if (!Guid::TryParse(DisplayedSpeaker.substr(10), Key.Asset))
    {
        return nullptr;
    }
    auto& Assets = *GetScene()->GetAssetManager();
    Assets.LoadAsync<CharacterDefinition>(Key);
    AssetHandle<CharacterDefinition> Loaded;
    if (Assets.TryGetLoaded(Key, Loaded))
    {
        return Loaded.GetSharedResource();
    }
    return nullptr;
}

std::string StoryRuntime::GetDisplayedSpeaker() const
{
    if (const auto Character = GetSpeakerCharacter())
    {
        return Character->Name;
    }
    if (DisplayedSpeaker.rfind("character:", 0) == 0)
    {
        return "Персонаж недоступен";
    }
    return DisplayedSpeaker;
}

std::string StoryRuntime::GetDisplayedDialogueWidget() const
{
    if (const auto Character = GetSpeakerCharacter())
    {
        return Character->DialogueWidget;
    }
    return {};
}

namespace
{
float Clamp01(float Value)
{
    if (Value < 0.0f)
    {
        return 0.0f;
    }
    if (Value > 1.0f)
    {
        return 1.0f;
    }
    return Value;
}

Vector3 LerpVector3(const Vector3& Start, const Vector3& End, float Alpha)
{
    return Start + (End - Start) * Alpha;
}

Quaternion SlerpQuaternion(const Quaternion& Start, const Quaternion& End, float Alpha)
{
    return Quaternion::Slerp(Start, End, Alpha);
}
}

StoryRuntime::StoryRuntime() = default;
StoryRuntime::~StoryRuntime()
{
    Stop();
}

void StoryRuntime::BindScene(Scene* InScene)
{
    Stop();
    BoundScene = {};
    if (InScene != nullptr)
    {
        BoundScene = InScene->GetObjectHandle();
    }
}

bool StoryRuntime::LoadFromFile(const std::filesystem::path& AbsolutePath)
{
    StoryDocument Loaded{};
    const StorySerializeResult LoadedResult = StoryDocumentIO::LoadFromFile(AbsolutePath, Loaded);
    if (!LoadedResult.bOk)
    {
        LastError = LoadedResult.Error;
        PrintString(std::string("StoryRuntime: load failed: ") + LastError);
        return false;
    }
    return LoadDocument(Loaded);
}

bool StoryRuntime::LoadDocument(const StoryDocument& InDocument)
{
    if (!ValidateStoryEvents(InDocument, LastError)) { return false; }
    if (!ValidateNarrativeDocument(InDocument, LastError))
    {
        return false;
    }
    StoryDocument ResolvedDocument = InDocument;
    for (StoryNode& Node : ResolvedDocument.Nodes)
    {
        for (StoryActionDefinition& Action : Node.Actions)
        {
            if (!Action.TargetObjectId.empty() || Action.TargetObjectName.empty())
            {
                continue;
            }
            GameObject* Target = nullptr;
            Scene* World = GetScene();
            if (World != nullptr)
            {
                for (GameObject* Candidate : World->GetAllObjects())
                {
                    if (Candidate->GetName() == Action.TargetObjectName)
                    {
                        if (Target != nullptr)
                        {
                            LastError = "Ambiguous legacy story target: " + Action.TargetObjectName;
                            return false;
                        }
                        Target = Candidate;
                    }
                }
            }
            if (Target == nullptr)
            {
                LastError = "Legacy story target not found: " + Action.TargetObjectName;
                return false;
            }
            Action.TargetObjectId = Target->GetPersistentId();
            Action.TargetObjectName.clear();
        }
    }
    Document = std::move(ResolvedDocument);
    LastError.clear();
    Reset();
    PrintString(std::string("StoryRuntime: loaded story ") + Document.Name);
    return true;
}

void StoryRuntime::Reset()
{
    Narrative.Reset(BoundScene);
    bNarrativeRunning = false;
    Events.reset();
    StagingPhase = 0;
    LastError.clear();
    CancelAllActions();
    Phase = StoryPlaybackPhase::Idle;
    DisplayedSpeaker.clear();
    DisplayedLine.clear();
    ChoicePrompt.clear();
    PendingChoices.clear();
    CurrentNodeId.clear();

    if (Document.StartNodeId.empty())
    {
        if (Document.Nodes.empty())
        {
            Phase = StoryPlaybackPhase::End;
        }
        return;
    }

    EnterNode(Document.StartNodeId);
}

void StoryRuntime::Stop()
{
    Narrative.Stop();
    bNarrativeRunning = false;
    Events.reset();
    StagingPhase = 0;
    LastError.clear();
    CancelAllActions();
    Phase = StoryPlaybackPhase::Idle;
    DisplayedSpeaker.clear();
    DisplayedLine.clear();
    PendingChoices.clear();
    ChoicePrompt.clear();
    CurrentNodeId.clear();
}

bool StoryRuntime::IsActive() const
{
    return Phase != StoryPlaybackPhase::Idle && Phase != StoryPlaybackPhase::End && Phase != StoryPlaybackPhase::Failed;
}

bool StoryRuntime::IsFinished() const
{
    return Phase == StoryPlaybackPhase::End;
}

std::vector<StoryEventSnapshot> StoryRuntime::GetEventSnapshots() const
{
    AssertGameThread();
    return Events ? Events->GetSnapshots() : std::vector<StoryEventSnapshot>{};
}

bool StoryRuntime::SetEventPaused(uint64_t InstanceId, bool bPaused)
{
    AssertGameThread();
    return Events && Events->SetPaused(InstanceId, bPaused);
}

bool StoryRuntime::StopEvent(uint64_t InstanceId)
{
    AssertGameThread();
    return Events && Events->Stop(InstanceId);
}

bool StoryRuntime::CanAdvanceDialogue() const
{
    return Phase == StoryPlaybackPhase::Line && !bActionSequenceRunning &&
        StagingPhase != 3 && (!Events || Events->IsReady());
}

bool StoryRuntime::CanSelectChoice() const
{
    return Phase == StoryPlaybackPhase::Choice && StagingPhase != 3 && (!Events || Events->IsReady());
}

bool StoryRuntime::AdvanceDialogue()
{
    if (!CanAdvanceDialogue())
    {
        return false;
    }

    const StoryNode* Node = FindNode(CurrentNodeId);
    if (Node == nullptr || Node->Kind != StoryNodeKind::Line)
    {
        return false;
    }

    FinishDialogue(Node->NextNodeId);
    return true;
}

bool StoryRuntime::SelectChoice(size_t ChoiceIndex)
{
    if (!CanSelectChoice() || ChoiceIndex >= PendingChoices.size())
    {
        return false;
    }

    const StoryNodeId NextNodeId = PendingChoices[ChoiceIndex].NextNodeId;
    PendingChoices.clear();
    ChoicePrompt.clear();
    FinishDialogue(NextNodeId);
    return true;
}

bool StoryRuntime::TriggerEvent(const std::string& EventId)
{
    AssertGameThread();
    const auto* World = GetScene();
    if (!World || !World->IsPlaying() || !FindStoryEvent(Document, EventId))
    {
        return false;
    }
    if (!Events)
    {
        Events = std::make_unique<StoryEventPlayer>(BoundScene, NextEventInstanceId);
    }
    return Events->Trigger(Document, EventId);
}

void StoryRuntime::Tick(float DeltaTime)
{
    if (!std::isfinite(DeltaTime) || DeltaTime < 0) { return; }
    if (Events)
    {
        Events->Tick(DeltaTime);
        if (!Events->GetError().empty())
        {
            const auto Error = Events->GetError();
            Events.reset();
            FailAction(Error);
            return;
        }
        if (Events->IsReady())
        {
            if (StagingPhase == 1) { DisplayNode(); }
            else if (StagingPhase == 3)
            {
                const auto Next = StagedNext;
                StagingPhase = 0;
                if (Next.empty()) { Phase = StoryPlaybackPhase::End; Events.reset(); }
                else { EnterNode(Next); }
                return;
            }
        }
    }
    if (bNarrativeRunning)
    {
        bool Completed = false;
        std::string Exit;
        std::string Error;
        if (!Narrative.Tick(DeltaTime, Completed, Exit, Error))
        {
            FailAction(Error);
            return;
        }
        if (Completed)
        {
            bNarrativeRunning = false;
            StoryNodeId Next;
            for (const StoryChoice& Branch : FindNode(CurrentNodeId)->Choices)
            {
                if (Branch.Id == Exit)
                {
                    Next = Branch.NextNodeId;
                    break;
                }
            }
            if (Next.empty())
            {
                Phase = StoryPlaybackPhase::End;
                Events.reset();
                Narrative.Stop();
            }
            else
            {
                EnterNode(Next);
            }
        }
        return;
    }
    if (Phase != StoryPlaybackPhase::Action || !bActionSequenceRunning)
    {
        return;
    }

    if (ActiveActionIndex >= ActionQueue.size())
    {
        bActionSequenceRunning = false;
        const StoryNode* Node = FindNode(CurrentNodeId);
        if (Node != nullptr && !Node->NextNodeId.empty())
        {
            EnterNode(Node->NextNodeId);
        }
        else
        {
            Phase = StoryPlaybackPhase::End;
            Events.reset();
        }
        return;
    }

    ActiveAction& Action = ActionQueue[ActiveActionIndex];
    if (Action.State == StoryActionState::Failed || Action.State == StoryActionState::Cancelled)
    {
        CompleteActiveAction();
        return;
    }
    if (Action.State != StoryActionState::Running)
    {
        return;
    }

    TickActiveAction(DeltaTime);
}

const StoryChoice& StoryRuntime::GetChoice(size_t ChoiceIndex) const
{
    return PendingChoices.at(ChoiceIndex);
}

const StoryNode* StoryRuntime::FindNode(const StoryNodeId& NodeId) const
{
    for (const StoryNode& Node : Document.Nodes)
    {
        if (Node.Id == NodeId)
        {
            return &Node;
        }
    }
    return nullptr;
}

void StoryRuntime::EnterNode(const StoryNodeId& NodeId)
{
    if (Document.Nodes.empty() && NodeId.empty())
    {
        Phase = StoryPlaybackPhase::End;
        return;
    }
    const StoryNode* Node = FindNode(NodeId);
    if (Node == nullptr)
    {
        LastError = "Story node not found: " + NodeId;
        Phase = StoryPlaybackPhase::Failed;
        PrintString(std::string("StoryRuntime: ") + LastError);
        return;
    }

    CurrentNodeId = NodeId;

    if (Node->Kind == StoryNodeKind::Line || Node->Kind == StoryNodeKind::Choice)
    {
        if (!Events && !Node->EventBatches.empty())
        {
            Events = std::make_unique<StoryEventPlayer>(BoundScene, NextEventInstanceId);
        }
        if (Events)
        {
            StagingPhase = 1;
            Phase = StoryPlaybackPhase::Action;
            DisplayedSpeaker.clear(); DisplayedLine.clear(); PendingChoices.clear(); ChoicePrompt.clear();
            Events->BeginPhase(Document, *Node, StoryEventPhase::Before);
            if (!Events->IsReady()) { return; }
        }
    }
    DisplayNode();
}

void StoryRuntime::FinishDialogue(const StoryNodeId& Next)
{
    if (Events)
    {
        StagedNext = Next;
        StagingPhase = 3;
        Events->BeginPhase(Document, *FindNode(CurrentNodeId), StoryEventPhase::After);
        if (!Events->IsReady()) { return; }
    }
    StagingPhase = 0;
    if (Next.empty()) { Phase = StoryPlaybackPhase::End; Events.reset(); }
    else { EnterNode(Next); }
}

void StoryRuntime::DisplayNode()
{
    const auto* Node = FindNode(CurrentNodeId);
    if (!Node) { return; }

    switch (Node->Kind)
    {
    case StoryNodeKind::Line:
        Phase = StoryPlaybackPhase::Line;
        DisplayedSpeaker = Node->Speaker;
        DisplayedLine = Node->Text;
        break;
    case StoryNodeKind::Choice:
        Phase = StoryPlaybackPhase::Choice;
        ChoicePrompt = Node->Prompt;
        if (ChoicePrompt.empty())
        {
            ChoicePrompt = Node->Text;
        }
        DisplayedSpeaker.clear();
        DisplayedLine = ChoicePrompt;
        PendingChoices = Node->Choices;
        break;
    case StoryNodeKind::Action:
        Phase = StoryPlaybackPhase::Action;
        DisplayedSpeaker.clear();
        DisplayedLine.clear();
        BeginActionSequence();
        break;
    case StoryNodeKind::Narrative:
    {
        Phase = StoryPlaybackPhase::Action;
        DisplayedSpeaker.clear();
        DisplayedLine.clear();
        PendingChoices.clear();
        std::string Error;
        if (!Narrative.Start(Document, *Node, Error))
        {
            FailAction(Error);
            return;
        }
        bNarrativeRunning = true;
        break;
    }
    case StoryNodeKind::End:
        Narrative.Stop();
        Events.reset();
        StagingPhase = 0;
        Phase = StoryPlaybackPhase::End;
        DisplayedSpeaker.clear();
        DisplayedLine.clear();
        break;
    }
    if (Events && StagingPhase == 1)
    {
        StagingPhase = 2;
        Events->BeginPhase(Document, *Node, StoryEventPhase::OnStart);
    }
}

void StoryRuntime::BeginActionSequence()
{
    CancelAllActions();

    const StoryNode* Node = FindNode(CurrentNodeId);
    if (Node == nullptr)
    {
        Phase = StoryPlaybackPhase::End;
        return;
    }

    ActionQueue.clear();
    for (const StoryActionDefinition& Definition : Node->Actions)
    {
        ActiveAction Action{};
        Action.Definition = Definition;
        ActionQueue.push_back(std::move(Action));
    }

    ActiveActionIndex = 0;
    bActionSequenceRunning = true;
    if (ActionQueue.empty())
    {
        return;
    }

    PrepareActiveAction();
}

Scene* StoryRuntime::GetScene() const
{
    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (Memory == nullptr)
    {
        return nullptr;
    }
    return Memory->ResolveHandle<Scene>(BoundScene);
}

void StoryRuntime::FailAction(const std::string& Error)
{
    Narrative.Stop();
    bNarrativeRunning = false;
    Events.reset();
    LastError = Error;
    CancelAllActions();
    Phase = StoryPlaybackPhase::Failed;
    PrintString("StoryRuntime: " + Error);
}

void StoryRuntime::PrepareActiveAction()
{
    ActiveAction& Action = ActionQueue[ActiveActionIndex];
    if (Action.Definition.Kind == StoryActionKind::Wait)
    {
        return;
    }
    Scene* World = GetScene();
    if (World == nullptr)
    {
        FailAction("Story scene is no longer available");
        return;
    }
    GameObject* Target = World->FindByPersistentId(Action.Definition.TargetObjectId);
    if (Target != nullptr)
    {
        Action.TargetObject = Target->GetObjectHandle();
    }
    if (Action.Definition.Kind == StoryActionKind::CharacterAnimation)
    {
        if (!Target || !Target->GetComponent<CharacterComponent>())
        {
            FailAction("CharacterAnimation requires a character target");
        }
        return;
    }
    if (Action.Definition.Kind == StoryActionKind::NavigateTo)
    {
        NavigationAgentComponent* Agent = nullptr;
        if (Target != nullptr)
        {
            Agent = Target->GetComponent<NavigationAgentComponent>();
        }
        if (Agent == nullptr || !Agent->IsEnabled())
        {
            FailAction("NavigateTo requires an enabled navigation agent on its target");
            return;
        }
        Action.NavigationAgent = Agent->GetObjectHandle();
        Action.NavigationRequest = Agent->MoveTo(Action.Definition.TargetPosition);
        return;
    }
    if (Action.Definition.Kind == StoryActionKind::MoveTo)
    {
        if (Target == nullptr)
        {
            FailAction("MoveTo target not found: " + Action.Definition.TargetObjectId);
            return;
        }
        if (Target->bStoryControllingTransform) { FailAction("MoveTo target is already controlled by another story action"); return; }
        Target->bStoryControllingTransform = true;
        Action.bOwnsTransform = true;
        Action.StartPosition = Target->GetTransform().Position;
        return;
    }
    GameObject* Camera = ResolvePrimaryCameraObject();
    if (Camera == nullptr)
    {
        FailAction("CameraCut requires an active camera");
        return;
    }
    Action.PrimaryCameraObject = Camera->GetObjectHandle();
    if (Camera->bStoryControllingTransform) { FailAction("Camera is already controlled by another story action"); return; }
    Camera->bStoryControllingTransform = true;
    Action.bOwnsTransform = true;
    Action.StartPosition = Camera->GetTransform().Position;
    Action.StartRotation = Camera->GetTransform().Rotation;
    if (Action.Definition.bUseTargetObjectTransform)
    {
        if (Target == nullptr)
        {
            FailAction("CameraCut target not found: " + Action.Definition.TargetObjectId);
            return;
        }
        Matrix Destination = Target->GetWorldMatrix();
        if (Camera->GetParent() != nullptr)
        {
            Destination *= Camera->GetParent()->GetWorldMatrix().Invert();
        }
        Vector3 DestinationScale;
        if (!Destination.Decompose(DestinationScale, Action.Definition.TargetRotation, Action.Definition.TargetPosition))
        {
            FailAction("CameraCut target transform cannot be decomposed");
            return;
        }
        Action.Definition.bHasTargetRotation = true;
    }
}

void StoryRuntime::TickActiveAction(float DeltaTime)
{
    ActiveAction& Action = ActionQueue[ActiveActionIndex];
    if (Action.State != StoryActionState::Running)
    {
        return;
    }

    Action.ElapsedSeconds += DeltaTime;
    const float Duration = std::max(Action.Definition.DurationSeconds, 0.0f);
    if (Action.Definition.Kind == StoryActionKind::CharacterAnimation)
    {
        auto* World = GetScene();
        GameObject* Target = nullptr;
        if (World)
        {
            Target = World->FindByHandle(Action.TargetObject);
        }
        CharacterComponent* Character = nullptr;
        if (Target && Target->IsActiveInHierarchy())
        {
            Character = Target->GetComponent<CharacterComponent>();
        }
        if (!Character || !Character->IsEnabled() || Character->IsPendingRemoval())
        {
            FailAction("CharacterAnimation target is no longer available");
            return;
        }
        if (!Action.bAnimationStarted)
        {
            if (World->GetAssetManager())
            {
                Character->Resolve(*World->GetAssetManager());
            }
            if (!Character->GetDefinition())
            {
                Action.ElapsedSeconds = 0.f;
                if (Character->bMissingAsset)
                {
                    FailAction("CharacterAnimation asset is unavailable");
                }
                return;
            }
            if (!Character->PlayAnimation(Action.Definition.AnimationId))
            {
                FailAction("CharacterAnimation selection is missing or disabled");
                return;
            }
            Action.bAnimationStarted = true;
            Action.ElapsedSeconds = 0.f;
        }
        if (Action.ElapsedSeconds >= Duration)
        {
            CompleteActiveAction();
        }
        return;
    }
    if (Action.Definition.Kind == StoryActionKind::NavigateTo)
    {
        NavigationAgentComponent* Agent = MemorySubsystem::Get()->ResolveHandle<NavigationAgentComponent>(Action.NavigationAgent);
        if (Agent == nullptr || Agent->IsPendingRemoval() || !Agent->IsEnabled()
            || Agent->GetGameObject()->IsPendingDestroy() || !Agent->GetGameObject()->IsActiveInHierarchy())
        {
            FailAction("NavigateTo target is no longer available");
            return;
        }
        if (Agent->GetRequestId() != Action.NavigationRequest)
        {
            FailAction("NavigateTo request was replaced");
            return;
        }
        if (Duration > 0.f && Action.ElapsedSeconds > Duration)
        {
            Agent->Stop();
            FailAction("NavigateTo timed out");
            return;
        }
        const NavigationAgentState State = Agent->GetMovementState();
        if (State == NavigationAgentState::Reached)
        {
            CompleteActiveAction();
        }
        else if (State == NavigationAgentState::Failed || State == NavigationAgentState::Blocked
            || State == NavigationAgentState::Cancelled || State == NavigationAgentState::PartialReached)
        {
            FailAction("NavigateTo failed: " + Agent->GetDiagnostic());
        }
        return;
    }

    if (Action.Definition.Kind == StoryActionKind::Wait)
    {
        if (Action.ElapsedSeconds >= Duration)
        {
            CompleteActiveAction();
        }
        return;
    }

    if (Action.Definition.Kind == StoryActionKind::MoveTo)
    {
        Scene* World = GetScene();
        GameObject* Target = nullptr;
        if (World != nullptr)
        {
            Target = World->FindByHandle(Action.TargetObject);
        }
        if (Target == nullptr)
        {
            FailAction("Story action target was destroyed");
            return;
        }

        float Alpha = 1.0f;
        if (Duration > 0.0f)
        {
            Alpha = Clamp01(Action.ElapsedSeconds / Duration);
        }
        Transform ObjectTransform = Target->GetTransform();
        ObjectTransform.Position = LerpVector3(Action.StartPosition, Action.Definition.TargetPosition, Alpha);
        Target->m_Transform = ObjectTransform;

        if (Alpha >= 1.0f)
        {
            CompleteActiveAction();
        }
        return;
    }

    if (Action.Definition.Kind == StoryActionKind::CameraCut)
    {
        Scene* World = GetScene();
        GameObject* Camera = nullptr;
        if (World != nullptr)
        {
            Camera = World->FindByHandle(Action.PrimaryCameraObject);
        }
        if (Camera == nullptr)
        {
            FailAction("Story action target was destroyed");
            return;
        }

        float Alpha = 1.0f;
        if (Duration > 0.0f)
        {
            Alpha = Clamp01(Action.ElapsedSeconds / Duration);
        }
        Transform CameraTransform = Camera->GetTransform();
        CameraTransform.Position = LerpVector3(Action.StartPosition, Action.Definition.TargetPosition, Alpha);
        if (Action.Definition.bHasTargetRotation)
        {
            CameraTransform.Rotation = SlerpQuaternion(
                Action.StartRotation,
                Action.Definition.TargetRotation,
                Alpha);
        }
        Camera->m_Transform = CameraTransform;

        if (Alpha >= 1.0f)
        {
            CompleteActiveAction();
        }
    }
}

void StoryRuntime::CompleteActiveAction()
{
    if (ActiveActionIndex >= ActionQueue.size())
    {
        return;
    }

    ReleaseActionTransform(ActionQueue[ActiveActionIndex]);
    ActionQueue[ActiveActionIndex].State = StoryActionState::Completed;
    ++ActiveActionIndex;

    if (ActiveActionIndex >= ActionQueue.size())
    {
        return;
    }

    PrepareActiveAction();
}

void StoryRuntime::CancelAllActions()
{
    for (ActiveAction& Action : ActionQueue)
    {
        if (Action.State == StoryActionState::Running)
        {
            ReleaseActionTransform(Action);
            Action.State = StoryActionState::Cancelled;
        }
    }
    ActionQueue.clear();
    ActiveActionIndex = 0;
    bActionSequenceRunning = false;
}

void StoryRuntime::ReleaseActionTransform(ActiveAction& Action)
{
    if (Action.Definition.Kind == StoryActionKind::NavigateTo)
    {
        NavigationAgentComponent* Agent = MemorySubsystem::Get()->ResolveHandle<NavigationAgentComponent>(Action.NavigationAgent);
        if (Agent != nullptr && Agent->GetRequestId() == Action.NavigationRequest && Action.State == StoryActionState::Running)
        {
            const NavigationAgentState Movement = Agent->GetMovementState();
            if (Movement == NavigationAgentState::Moving || Movement == NavigationAgentState::WaitingForNavigation
                || Movement == NavigationAgentState::WaitingForLink)
            {
                Agent->Stop();
            }
        }
        return;
    }
    Scene* World = GetScene();
    if (World == nullptr)
    {
        return;
    }
    ObjectHandle Identity = Action.TargetObject;
    if (Action.Definition.Kind == StoryActionKind::CameraCut)
    {
        Identity = Action.PrimaryCameraObject;
    }
    GameObject* ControlledObject = World->FindByHandle(Identity);
    if (ControlledObject != nullptr && Action.bOwnsTransform)
    {
        ControlledObject->bStoryControllingTransform = false;
        Action.bOwnsTransform = false;
    }
}

GameObject* StoryRuntime::ResolvePrimaryCameraObject() const
{
    Scene* World = GetScene();
    if (World == nullptr)
    {
        return nullptr;
    }
    CameraComponent* Camera = World->FindPrimaryCamera();
    if (Camera == nullptr)
    {
        return nullptr;
    }
    return Camera->GetGameObject();
}
