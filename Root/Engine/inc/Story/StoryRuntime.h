#pragma once

#include "Story/StoryTypes.h"
#include "Gameplay/ObjectHandle.h"

#include <filesystem>
#include <string>
#include <vector>

class GameObject;
class Scene;

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
    void BindScene(Scene* InScene);

    bool LoadFromFile(const std::filesystem::path& AbsolutePath);
    bool LoadDocument(const StoryDocument& Document);

    void Reset();
    void Stop();

    bool IsActive() const;
    bool IsFinished() const;
    StoryPlaybackPhase GetPlaybackPhase() const { return Phase; }

    bool CanAdvanceDialogue() const;
    bool AdvanceDialogue();
    bool SelectChoice(size_t ChoiceIndex);
    void Tick(float DeltaTime);

    const std::string& GetDisplayedSpeaker() const { return DisplayedSpeaker; }
    const std::string& GetDisplayedLine() const { return DisplayedLine; }
    const std::string& GetChoicePrompt() const { return ChoicePrompt; }
    size_t GetChoiceCount() const { return PendingChoices.size(); }
    const StoryChoice& GetChoice(size_t ChoiceIndex) const;

    const std::string& GetLastError() const { return LastError; }

private:
    struct ActiveAction
    {
        StoryActionDefinition Definition;
        StoryActionState State = StoryActionState::Running;
        float ElapsedSeconds = 0.0f;
        Vector3 StartPosition = Vector3::Zero;
        Quaternion StartRotation = Quaternion::Identity;
        ObjectHandle TargetObject;
        ObjectHandle PrimaryCameraObject;
    };

    const StoryNode* FindNode(const StoryNodeId& NodeId) const;
    void EnterNode(const StoryNodeId& NodeId);
    void BeginActionSequence();
    void PrepareActiveAction();
    void FailAction(const std::string& Error);
    Scene* GetScene() const;
    void TickActiveAction(float DeltaTime);
    void CompleteActiveAction();
    void CancelAllActions();
    GameObject* ResolvePrimaryCameraObject() const;

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

    std::string LastError;
};
