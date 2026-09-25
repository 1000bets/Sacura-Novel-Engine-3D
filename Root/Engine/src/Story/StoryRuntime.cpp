#include "Story/StoryRuntime.h"

#include "Core/Threading/ThreadContext.h"
#include "Core/MemorySubsystem.h"
#include "Game/Scene.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/GameObject.h"
#include "Story/StoryDocumentIO.h"

#include <algorithm>
#include <cmath>

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
        return;
    }

    EnterNode(Document.StartNodeId);
}

void StoryRuntime::Stop()
{
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

bool StoryRuntime::CanAdvanceDialogue() const
{
    return Phase == StoryPlaybackPhase::Line && !bActionSequenceRunning;
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

    if (Node->NextNodeId.empty())
    {
        Phase = StoryPlaybackPhase::End;
        return true;
    }

    EnterNode(Node->NextNodeId);
    return true;
}

bool StoryRuntime::SelectChoice(size_t ChoiceIndex)
{
    if (Phase != StoryPlaybackPhase::Choice || ChoiceIndex >= PendingChoices.size())
    {
        return false;
    }

    const StoryNodeId NextNodeId = PendingChoices[ChoiceIndex].NextNodeId;
    PendingChoices.clear();
    ChoicePrompt.clear();
    EnterNode(NextNodeId);
    return true;
}

void StoryRuntime::Tick(float DeltaTime)
{
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
    const StoryNode* Node = FindNode(NodeId);
    if (Node == nullptr)
    {
        LastError = "Story node not found: " + NodeId;
        Phase = StoryPlaybackPhase::Failed;
        PrintString(std::string("StoryRuntime: ") + LastError);
        return;
    }

    CurrentNodeId = NodeId;

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
    case StoryNodeKind::End:
        Phase = StoryPlaybackPhase::End;
        DisplayedSpeaker.clear();
        DisplayedLine.clear();
        break;
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
    if (Action.Definition.Kind == StoryActionKind::MoveTo)
    {
        if (Target == nullptr)
        {
            FailAction("MoveTo target not found: " + Action.Definition.TargetObjectId);
            return;
        }
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
    Action.StartPosition = Camera->GetTransform().Position;
    Action.StartRotation = Camera->GetTransform().Rotation;
    if (Action.Definition.bUseTargetObjectTransform)
    {
        if (Target == nullptr)
        {
            FailAction("CameraCut target not found: " + Action.Definition.TargetObjectId);
            return;
        }
        Action.Definition.TargetPosition = Target->GetTransform().Position;
        Action.Definition.TargetRotation = Target->GetTransform().Rotation;
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
        Target->SetTransform(ObjectTransform);

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
        Camera->SetTransform(CameraTransform);

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
            Action.State = StoryActionState::Cancelled;
        }
    }
    ActionQueue.clear();
    ActiveActionIndex = 0;
    bActionSequenceRunning = false;
}

GameObject* StoryRuntime::ResolvePrimaryCameraObject() const
{
    Scene* World = GetScene();
    if (World == nullptr)
    {
        return nullptr;
    }

    GameObject* Fallback = nullptr;
    for (GameObject* ObjectInstance : World->GetAllObjects())
    {
        if (ObjectInstance == nullptr || !ObjectInstance->IsActiveInHierarchy())
        {
            continue;
        }
        CameraComponent* Camera = ObjectInstance->GetComponent<CameraComponent>();
        if (Camera == nullptr || !Camera->IsEnabled())
        {
            continue;
        }
        if (Camera->bPrimary)
        {
            return ObjectInstance;
        }
        if (Fallback == nullptr)
        {
            Fallback = ObjectInstance;
        }
    }
    return Fallback;
}
