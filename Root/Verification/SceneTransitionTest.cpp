#include "Engine.h"
#include "Assets/Guid.h"
#include "Core/EnginePaths.h"
#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Story/StoryDocumentIO.h"
#include "World/GameObject.h"
#include "World/Scene.h"
#include "World/Serialization/SceneSerializer.h"
#include <fstream>
#include <iostream>
#include <stdexcept>

void RequireTransition(bool bCondition, const std::string& Message)
{
    if (!bCondition)
    {
        throw std::runtime_error(Message);
    }
}

class SceneTransitionProbe : public Component
{
public:
    Engine* Runtime = nullptr;
    bool* bCallbackCompleted = nullptr;
    bool* bStopped = nullptr;

    void Tick(float) override
    {
        std::string Error;
        RequireTransition(Runtime->RequestScene("/Game/Scenes/Second.scene", Error), Error);
        RequireTransition(GetGameObject()->GetScene() == Runtime->GetActiveScene(), "World changed inside a component callback");
        *bCallbackCompleted = true;
    }

    void EndPlay() override
    {
        *bStopped = true;
    }
};

int main(int ArgumentCount, char** Arguments)
{
    SetCurrentThreadRole(ThreadRole::Game);
    const auto Executable = std::filesystem::absolute(std::filesystem::u8path(Arguments[0]));
    EnginePaths::InitializeFromExecutable(Executable);
    const auto FixtureRoot = Executable.parent_path() / ("SceneTransition-" + Guid::Generate().ToString());
    if (FixtureRoot.root_name() == "C:")
    {
        std::cerr << "Scene transition fixtures require a drive outside C:\n";
        return 1;
    }
    const auto Content = FixtureRoot / "Content";
    std::filesystem::create_directories(Content / "Scenes");
    std::filesystem::create_directories(Content / "Stories");
    std::filesystem::create_directories(Content / "Scripts");
    std::ofstream(Content / "Scripts" / "scene_transition_probe.py") << R"PY(import engine
from pathlib import Path

@engine.register_node(type_id='test.SceneTransition', label='Scene transition test')
class SceneTransition(engine.NarrativeNode):
    Exits = ['Done']

    def Execute(Self, Context):
        Marker = Path(__file__).parent / 'transition.txt'
        engine.RequestScene('/Game/Scenes/Second.scene', '/Game/Stories/Arrival.story')
        assert engine.IsSceneTransitionPending()
        assert engine.GetSceneTransitionError() == ''
        Marker.write_text('requested', encoding='utf-8')
        try:
            yield Context.Wait(60)
        finally:
            Marker.write_text('closed', encoding='utf-8')
        return engine.NarrativeResult('Done')
)PY";
    Engine Runtime;
    Runtime.InitializeHeadless(Content);
    int Result = 0;
    try
    {
        RequireTransition(ArgumentCount == 1 && Runtime.IsInitialized(), Runtime.GetInitializationError());
        if (Runtime.GetScripting().IsPythonEnabled())
        {
            RequireTransition(Runtime.GetScripting().ImportModule("scene_transition_probe").bOk, "Python probe import failed");
        }
        const auto MakeScene = [](const std::string& Name)
        {
            std::unique_ptr<Scene> World(MemorySubsystem::Get()->NewObject<Scene>());
            World->CreateGameObject(Name);
            return World;
        };
        auto First = MakeScene("First");
        auto Second = MakeScene("Second");
        RequireTransition(SceneSerializer::SerializeToFile(*First, Content / "Scenes" / "First.scene").bOk, "First scene save failed");
        RequireTransition(SceneSerializer::SerializeToFile(*Second, Content / "Scenes" / "Second.scene").bOk, "Second scene save failed");
        Second.reset();
        std::ofstream(Content / "Scenes" / "Broken.scene") << "invalid scene";
        std::ofstream(Content / "Stories" / "BrokenOpening.story") << "invalid story";
        StoryDocument Arrival;
        Arrival.StartNodeId = "arrival";
        StoryNode Line;
        Line.Id = "arrival";
        Line.Text = "Second scene story";
        Arrival.Nodes.push_back(Line);
        RequireTransition(StoryDocumentIO::SaveToFile(Arrival, Content / "Stories" / "Arrival.story").bOk, "Story save failed");
        Runtime.GetAssetRegistry().ScanContent();
        Runtime.AdoptScene(std::move(First));
        std::string Error;
        RequireTransition(!Runtime.RequestScene("/Game/Scenes/Second.scene", Error), "Editing must reject runtime transitions");
        RequireTransition(Runtime.StartGame(), "Player startup failed");
        const auto FirstIdentity = Runtime.GetActiveScene()->GetObjectHandle();
        auto* PlayerInput = Runtime.GetInputSession();
        RequireTransition(!Runtime.RequestScene("/Game/Scenes/Missing.scene", Error), "Missing scene must be rejected");
        RequireTransition(!Runtime.RequestScene("/Game/Stories/Arrival.story", Error), "Wrong asset type must be rejected");
        RequireTransition(!Runtime.RequestScene("/Game/Scenes/Second.scene", Error, "/Game/Stories/Missing.story"), "Missing story must be rejected");
        bool bInputCallbackCompleted = false;
        const auto AdvanceSubscription = PlayerInput->BindAction("engine.input.advance", InputPhase::Triggered, [&](const InputActionEvent&)
        {
            RequireTransition(Runtime.RequestScene("/Game/Scenes/Second.scene", Error, "/Game/Stories/Arrival.story"), Error);
            RequireTransition(Runtime.GetActiveScene()->GetObjectHandle() == FirstIdentity, "World changed inside an input callback");
            RequireTransition(!Runtime.RequestScene("/Game/Scenes/First.scene", Error), "Pending request must not be overwritten");
            bInputCallbackCompleted = true;
        });
        PlayerInput->Feed("test", "Keyboard", "Space", {1, 0, 0});
        Runtime.Tick(0.016f);
        RequireTransition(bInputCallbackCompleted && Runtime.GetActiveScene()->FindByName("Second"), "Input transition failed");
        RequireTransition(MemorySubsystem::Get()->ResolveHandle(FirstIdentity) == nullptr, "Previous world must be destroyed");
        RequireTransition(Runtime.GetInputSession() == PlayerInput && Runtime.IsGameRunning(), "Player session must survive a scene transition");
        RequireTransition(Runtime.GetStoryRuntime().GetDisplayedLine() == Line.Text, "New story must bind to the destination");
        PlayerInput->Unbind(AdvanceSubscription);
        int HeldTriggerCount = 0;
        PlayerInput->BindAction("engine.input.advance", InputPhase::Triggered, [&](const InputActionEvent&)
        {
            ++HeldTriggerCount;
        });
        PlayerInput->Feed("test", "Keyboard", "Space", {1, 0, 0});
        Runtime.Tick(0.016f);
        RequireTransition(HeldTriggerCount == 0, "Held input must require neutral after a transition");
        PlayerInput->Feed("test", "Keyboard", "Space", {});
        Runtime.Tick(0.016f);
        PlayerInput->Feed("test", "Keyboard", "Space", {1, 0, 0});
        Runtime.Tick(0.016f);
        RequireTransition(HeldTriggerCount == 1, "Input must work after returning to neutral");
        const auto SecondIdentity = Runtime.GetActiveScene()->GetObjectHandle();
        RequireTransition(Runtime.RequestScene("/Game/Scenes/Broken.scene", Error), Error);
        Runtime.Tick(0.016f);
        RequireTransition(Runtime.GetActiveScene()->GetObjectHandle() == SecondIdentity && !Runtime.GetSceneTransitionError().empty(), "Invalid scene must preserve the running world");
        RequireTransition(Runtime.RequestScene("/Game/Scenes/First.scene", Error, "/Game/Stories/BrokenOpening.story"), Error);
        Runtime.Tick(0.016f);
        RequireTransition(Runtime.GetActiveScene()->GetObjectHandle() == SecondIdentity && !Runtime.GetSceneTransitionError().empty(), "Invalid story must preserve the running world");
        Runtime.SetGamePaused(true);
        RequireTransition(Runtime.RequestScene("/Game/Scenes/First.scene", Error), Error);
        Runtime.Tick(0.016f);
        RequireTransition(Runtime.GetActiveScene()->FindByName("First") && Runtime.IsGamePaused(), "Transition must preserve Player pause");
        RequireTransition(Runtime.GetStoryRuntime().GetCurrentNodeId().empty(), "Transition without story must clear the previous story");
        RequireTransition(Runtime.RequestScene("/Game/Scenes/Second.scene", Error), Error);
        Runtime.StopGame();
        RequireTransition(!Runtime.IsSceneTransitionPending(), "Stop must cancel a queued transition");
        const auto EditIdentity = Runtime.GetEditScene()->GetObjectHandle();
        RequireTransition(Runtime.GetPlaySession().StartPlay(), "Editor Play startup failed");
        bool bComponentCallbackCompleted = false;
        bool bComponentStopped = false;
        auto* Probe = MemorySubsystem::Get()->NewObject<SceneTransitionProbe>();
        Probe->Runtime = &Runtime;
        Probe->bCallbackCompleted = &bComponentCallbackCompleted;
        Probe->bStopped = &bComponentStopped;
        Runtime.GetActiveScene()->CreateGameObject("Probe")->AddExistingComponent(Probe);
        Runtime.Tick(0.016f);
        RequireTransition(bComponentCallbackCompleted && bComponentStopped, "Component transition must finish the callback and end the old lifecycle");
        RequireTransition(Runtime.GetActiveScene()->FindByName("Second"), "PlayWorld must switch scenes");
        RequireTransition(Runtime.GetEditScene()->GetObjectHandle() == EditIdentity && Runtime.GetEditScene()->FindByName("First"), "EditWorld must remain intact");
        Runtime.GetPlaySession().Pause();
        RequireTransition(Runtime.RequestScene("/Game/Scenes/First.scene", Error), Error);
        Runtime.Tick(0.016f);
        RequireTransition(Runtime.GetPlaySession().GetState() == PlaySessionState::Paused, "Transition must preserve Editor pause");
        Runtime.GetPlaySession().StopPlay();
        RequireTransition(Runtime.GetActiveScene()->GetObjectHandle() == EditIdentity, "Stop Play must restore EditWorld");
        if (Runtime.GetScripting().IsPythonEnabled())
        {
            RequireTransition(Runtime.StartGame(), "Player restart failed");
            StoryDocument Narrative;
            Narrative.StartNodeId = "travel";
            StoryNode Travel;
            Travel.Id = "travel";
            Travel.Kind = StoryNodeKind::Narrative;
            Travel.NarrativeType = "test.SceneTransition";
            Travel.Choices.push_back({"Done", "", "Done"});
            Narrative.Nodes.push_back(Travel);
            RequireTransition(Runtime.GetStoryRuntime().LoadDocument(Narrative), Runtime.GetStoryRuntime().GetLastError());
            Runtime.Tick(0.016f);
            RequireTransition(Runtime.GetActiveScene()->FindByName("Second"), "Python Narrative must request a scene transition");
            std::string Marker;
            std::ifstream(Content / "Scripts" / "transition.txt") >> Marker;
            RequireTransition(Marker == "closed", "Narrative generator must close after returning from Execute");
            RequireTransition(Runtime.GetStoryRuntime().GetDisplayedLine() == Line.Text, "Narrative transition must start the destination story");
        }
        std::cout << "Scene transitions: Player, Editor Play, pause, input, component, Python Narrative and failed load passed\n";
    }
    catch (const std::exception& Failure)
    {
        std::cerr << Failure.what() << '\n';
        Result = 1;
    }
    Runtime.Shutdown();
    return Result;
}
