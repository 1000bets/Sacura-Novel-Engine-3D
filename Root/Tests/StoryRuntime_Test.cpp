#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Game/Scene.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/GameObject.h"
#include "Story/StoryDocumentIO.h"
#include "Story/StoryRuntime.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
int FailureCount = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++FailureCount;
        std::cout << "FAIL: " << Message << '\n';
    }
    else
    {
        std::cout << "  OK: " << Message << '\n';
    }
}

bool NearlyEqual(float Left, float Right, float Epsilon = 0.01f)
{
    return std::fabs(Left - Right) <= Epsilon;
}

StoryDocument BuildTestDocument()
{
    StoryDocument Document{};
    Document.Name = "Test";
    Document.StartNodeId = "line_a";

    StoryNode LineA{};
    LineA.Id = "line_a";
    LineA.Kind = StoryNodeKind::Line;
    LineA.Speaker = "Hero";
    LineA.Text = "First line";
    LineA.NextNodeId = "line_b";
    Document.Nodes.push_back(LineA);

    StoryNode LineB{};
    LineB.Id = "line_b";
    LineB.Kind = StoryNodeKind::Line;
    LineB.Speaker = "Hero";
    LineB.Text = "Second line";
    LineB.NextNodeId = "choice_a";
    Document.Nodes.push_back(LineB);

    StoryNode Choice{};
    Choice.Id = "choice_a";
    Choice.Kind = StoryNodeKind::Choice;
    Choice.Prompt = "Pick one";
    StoryChoice ChoiceLeft{};
    ChoiceLeft.Label = "Left";
    ChoiceLeft.NextNodeId = "action_a";
    Choice.Choices.push_back(ChoiceLeft);
    Document.Nodes.push_back(Choice);

    StoryNode Action{};
    Action.Id = "action_a";
    Action.Kind = StoryNodeKind::Action;
    StoryActionDefinition WaitAction{};
    WaitAction.Kind = StoryActionKind::Wait;
    WaitAction.DurationSeconds = 0.2f;
    Action.Actions.push_back(WaitAction);
    StoryActionDefinition MoveAction{};
    MoveAction.Kind = StoryActionKind::MoveTo;
    MoveAction.TargetObjectName = "Mover";
    MoveAction.TargetPosition = Vector3(2.0f, 0.0f, 0.0f);
    MoveAction.DurationSeconds = 0.5f;
    Action.Actions.push_back(MoveAction);
    Action.NextNodeId = "end_a";
    Document.Nodes.push_back(Action);

    StoryNode End{};
    End.Id = "end_a";
    End.Kind = StoryNodeKind::End;
    Document.Nodes.push_back(End);

    return Document;
}

void TestDialogueOrderAndActions()
{
    std::cout << "\n=== Story dialogue order and actions ===\n";
    MemorySubsystem Memory;
    Memory.Initialize();

    Scene* World = Memory.NewObject<Scene>("StoryWorld");
    GameObject* Mover = World->CreateGameObject("Mover");
    World->CreateGameObject("Main Camera")->AddComponent<CameraComponent>();

    StoryRuntime Runtime;
    Runtime.BindScene(World);
    Expect(Runtime.LoadDocument(BuildTestDocument()), "Load in-memory document");
    Expect(Runtime.GetPlaybackPhase() == StoryPlaybackPhase::Line, "Starts on line");
    Expect(Runtime.GetDisplayedLine() == "First line", "First line text");

    Expect(Runtime.AdvanceDialogue(), "Advance to second line");
    Expect(Runtime.GetDisplayedLine() == "Second line", "Second line text");

    Expect(Runtime.AdvanceDialogue(), "Advance to choice");
    Expect(Runtime.GetPlaybackPhase() == StoryPlaybackPhase::Choice, "At choice");
    Expect(Runtime.GetChoiceCount() == 1, "One choice");

    Expect(Runtime.SelectChoice(0), "Select choice");
    Expect(Runtime.GetPlaybackPhase() == StoryPlaybackPhase::Action, "Running actions");

    for (int Step = 0; Step < 80 && !Runtime.IsFinished(); ++Step)
    {
        Runtime.Tick(0.05f);
    }

    Expect(Runtime.IsFinished(), "Story finished after actions");
    Expect(NearlyEqual(Mover->GetTransform().Position.x, 2.0f), "MoveTo reached target X");

    Memory.DestroyObject(World);
    Memory.Deinitialize();
}

void TestCancelOnStop()
{
    std::cout << "\n=== Story cancel on stop ===\n";
    MemorySubsystem Memory;
    Memory.Initialize();

    Scene* World = Memory.NewObject<Scene>("StoryWorldCancel");
    World->CreateGameObject("Mover");
    World->CreateGameObject("Main Camera")->AddComponent<CameraComponent>();

    StoryRuntime Runtime;
    Runtime.BindScene(World);
    Expect(Runtime.LoadDocument(BuildTestDocument()), "Load document");
    Runtime.AdvanceDialogue();
    Runtime.AdvanceDialogue();
    Runtime.SelectChoice(0);
    Expect(Runtime.GetPlaybackPhase() == StoryPlaybackPhase::Action, "Action running");

    Runtime.Stop();
    Expect(Runtime.GetPlaybackPhase() == StoryPlaybackPhase::Idle, "Stop returns to idle");
    Expect(!Runtime.IsActive(), "Not active after stop");

    Runtime.Reset();
    Expect(Runtime.GetPlaybackPhase() == StoryPlaybackPhase::Line, "Reset returns to start line");

    Memory.DestroyObject(World);
    Memory.Deinitialize();
}

void TestPersistentTargetAndDeletion()
{
    MemorySubsystem Memory;
    Memory.Initialize();
    Scene* World = Memory.NewObject<Scene>("PersistentWorld");
    GameObject* Mover = World->CreateGameObject("Mover");
    World->CreateGameObject("Mover");
    StoryDocument Document = BuildTestDocument();
    Document.StartNodeId = "action_a";
    StoryActionDefinition& Move = Document.Nodes[3].Actions[1];
    Move.TargetObjectId = Mover->GetPersistentId();
    Move.TargetObjectName.clear();
    StoryRuntime Runtime;
    Runtime.BindScene(World);
    Expect(Runtime.LoadDocument(Document), "Duplicate names do not affect persistent targets");
    Mover->SetName("Renamed");
    Runtime.Tick(0.3f);
    Runtime.Tick(0.1f);
    Expect(Mover->GetTransform().Position.x > 0.0f, "Rename does not break a running action");
    World->DestroyGameObject(Mover);
    Runtime.Tick(0.1f);
    Expect(Runtime.GetPlaybackPhase() == StoryPlaybackPhase::Failed, "Destroyed target fails safely");
    Expect(!Runtime.GetLastError().empty(), "Destroyed target produces a diagnostic");
    Runtime.BindScene(nullptr);
    Memory.DestroyObject(World);
    Memory.Deinitialize();
}

void TestLegacyAmbiguousTarget()
{
    MemorySubsystem Memory;
    Memory.Initialize();
    Scene* World = Memory.NewObject<Scene>("AmbiguousWorld");
    World->CreateGameObject("Mover");
    World->CreateGameObject("Mover");
    StoryRuntime Runtime;
    Runtime.BindScene(World);
    Expect(!Runtime.LoadDocument(BuildTestDocument()), "Ambiguous legacy names are rejected");
    Memory.DestroyObject(World);
    Memory.Deinitialize();
}

void TestJsonRoundTrip()
{
    std::cout << "\n=== Story JSON round trip ===\n";
    const std::string JsonText = R"({
  "format": "sakura.story",
  "formatVersion": 1,
  "name": "RoundTrip",
  "startNodeId": "line_1",
  "nodes": [
    {
      "id": "line_1",
      "kind": "Line",
      "speaker": "A",
      "text": "Hi",
      "nextNodeId": "end_1"
    },
    {
      "id": "end_1",
      "kind": "End"
    }
  ]
})";

    StoryDocument Loaded{};
    const StorySerializeResult LoadResult = StoryDocumentIO::LoadFromJson(JsonText, Loaded);
    Expect(LoadResult.bOk, "Parse sakura.story JSON");
    Expect(Loaded.StartNodeId == "line_1", "Start node id preserved");

    std::string Saved{};
    const StorySerializeResult SaveResult = StoryDocumentIO::SaveToJson(Loaded, Saved);
    Expect(SaveResult.bOk, "Save to JSON");

    StoryDocument Reloaded{};
    const StorySerializeResult ReloadResult = StoryDocumentIO::LoadFromJson(Saved, Reloaded);
    Expect(ReloadResult.bOk, "Reload saved JSON");
    Expect(Reloaded.Nodes.size() == 2, "Node count preserved");
}
}

int main()
{
    SetCurrentThreadRole(ThreadRole::Game);
    TestJsonRoundTrip();
    TestDialogueOrderAndActions();
    TestCancelOnStop();
    TestPersistentTargetAndDeletion();
    TestLegacyAmbiguousTarget();

    if (FailureCount == 0)
    {
        std::cout << "\nAll story runtime tests passed.\n";
        return 0;
    }

    std::cout << "\n" << FailureCount << " story runtime test(s) failed.\n";
    return 1;
}
