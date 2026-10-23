#pragma once

#include "Story/StoryTypes.h"
#include "Story/Narrative.h"
#include "Core/Object/ObjectHandle.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class GameObject;
class Scene;
class StoryEventPlayer;
struct CharacterDefinition;

enum class StoryPlaybackPhase
{
    Idle = 0,
    Line,
    Choice,
    Action,
    End,
    Failed
};

class StoryRuntime
{
public:
    StoryRuntime();
    ~StoryRuntime();
    StoryRuntime(const StoryRuntime&) = delete;
    StoryRuntime& operator=(const StoryRuntime&) = delete;
    void BindScene(Scene* InScene);

    bool LoadFromFile(const std::filesystem::path& AbsolutePath);
    bool LoadDocument(const StoryDocument& Document);

    void Reset();
    void Stop();

    bool IsActive() const;
    bool IsFinished() const;
    StoryPlaybackPhase GetPlaybackPhase() const { return Phase; }

    bool CanAdvanceDialogue() const;
    bool CanSelectChoice() const;
    bool AdvanceDialogue();
    bool TriggerEvent(const std::string& EventId);
    bool SelectChoice(size_t ChoiceIndex);
    void Tick(float DeltaTime);

    std::vector<StoryEventSnapshot> GetEventSnapshots() const;
    bool SetEventPaused(uint64_t InstanceId, bool bPaused);
    bool StopEvent(uint64_t InstanceId);

    std::string GetDisplayedSpeaker() const;
    std::string GetDisplayedDialogueWidget() const;
    const std::string& GetDisplayedLine() const { return DisplayedLine; }
    const std::string& GetChoicePrompt() const { return ChoicePrompt; }
    size_t GetChoiceCount() const { return PendingChoices.size(); }
    const StoryChoice& GetChoice(size_t ChoiceIndex) const;

    const std::string& GetLastError() const { return LastError; }

private:
    std::shared_ptr<const CharacterDefinition> GetSpeakerCharacter() const;
    struct ActiveAction
    {
        StoryActionDefinition Definition;
        StoryActionState State = StoryActionState::Running;
        float ElapsedSeconds = 0.0f;
        Vector3 StartPosition = Vector3::Zero;
        Quaternion StartRotation = Quaternion::Identity;
        ObjectHandle TargetObject;
        ObjectHandle PrimaryCameraObject;
        ObjectHandle NavigationAgent;
        uint64_t NavigationRequest = 0;
        bool bOwnsTransform = false;
        bool bAnimationStarted = false;
    };

    const StoryNode* FindNode(const StoryNodeId& NodeId) const;
    void EnterNode(const StoryNodeId& NodeId);
    void DisplayNode();
    void FinishDialogue(const StoryNodeId& Next);
    void BeginActionSequence();
    void PrepareActiveAction();
    void FailAction(const std::string& Error);
    Scene* GetScene() const;
    void TickActiveAction(float DeltaTime);
    void CompleteActiveAction();
    void CancelAllActions();
    void ReleaseActionTransform(ActiveAction& Action);
    GameObject* ResolvePrimaryCameraObject() const;

    NarrativeRuntime Narrative;
    bool bNarrativeRunning = false;
    ObjectHandle BoundScene;
    StoryDocument Document;
    StoryNodeId CurrentNodeId;
    StoryPlaybackPhase Phase = StoryPlaybackPhase::Idle;

    std::string DisplayedSpeaker;
    std::string DisplayedLine;
    std::string ChoicePrompt;
    std::vector<StoryChoice> PendingChoices;

    std::vector<ActiveAction> ActionQueue;
    size_t ActiveActionIndex = 0;
    bool bActionSequenceRunning = false;
    std::unique_ptr<StoryEventPlayer> Events;
    uint64_t NextEventInstanceId = 1;
    // 0: legacy/no staging, 1: before text, 2: text visible, 3: after answer.
    int StagingPhase = 0;
    StoryNodeId StagedNext;

    std::string LastError;
};
