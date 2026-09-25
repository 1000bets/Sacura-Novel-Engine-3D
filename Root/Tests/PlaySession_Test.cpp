#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Game/PlaySession.h"
#include "Game/Scene.h"
#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
int Failures = 0;

void Expect(bool Condition, const char* Message)
{
    if (!Condition)
    {
        ++Failures;
        std::cout << "FAIL: " << Message << "\n" << std::flush;
        return;
    }
    std::cout << "PASS: " << Message << "\n" << std::flush;
}

bool NearlyEqual(float Left, float Right, float Epsilon = 0.001f)
{
    return std::fabs(Left - Right) <= Epsilon;
}

class TickProbeComponent : public Component
{
public:
    TickProbeComponent() = default;

    int TickCount = 0;

    void Tick(float DeltaTime) override
    {
        (void)DeltaTime;
        ++TickCount;
    }
};
}

int main()
{
    SetCurrentThreadRole(ThreadRole::Game);

    Engine BoundEngine;
    BoundEngine.InitializeHeadless({});

    MemorySubsystem* Memory = MemorySubsystem::Get();
    Expect(Memory != nullptr, "MemorySubsystem available");

    Scene* EditWorld = Memory->NewObject<Scene>("EditWorld");
    GameObject* EditObject = EditWorld->CreateGameObject("Mover");
    EditObject->GetTransform().Position = Vector3(1.f, 2.f, 3.f);
    BoundEngine.AdoptScene(std::unique_ptr<Scene>(EditWorld));

    PlaySession& Session = BoundEngine.GetPlaySession();

    Expect(Session.GetState() == PlaySessionState::Stopped, "Initial stopped");
    Expect(!Session.IsSimulating(), "Not simulating initially");

    Expect(Session.StartPlay(), "StartPlay");
    Expect(Session.IsSimulating(), "Simulating after start");
    Expect(Session.GetPlayWorld() != nullptr && Session.GetEditWorld() == EditWorld, "Edit/Play worlds");
    Expect(BoundEngine.GetActiveScene() == Session.GetPlayWorld(), "Active scene is play world");

    GameObject* PlayObject = Session.GetPlayWorld()->FindByName("Mover");
    Expect(PlayObject != nullptr, "Play object exists");
    PlayObject->GetTransform().Position = Vector3(99.f, 0.f, 0.f);

    Session.StopPlay();
    Expect(Session.GetState() == PlaySessionState::Stopped, "Stopped after StopPlay");
    Expect(BoundEngine.GetActiveScene() == EditWorld, "Active scene restored to edit");
    GameObject* EditObjectAfterStop = EditWorld->FindByName("Mover");
    Expect(EditObjectAfterStop != nullptr, "Edit object still present");
    Expect(NearlyEqual(EditObjectAfterStop->GetTransform().Position.x, 1.f), "Edit transform unchanged");
    Expect(NearlyEqual(EditObjectAfterStop->GetTransform().Position.y, 2.f), "Edit transform Y unchanged");

    Expect(Session.StartPlay(), "Second StartPlay");
    TickProbeComponent* Probe = Memory->NewObject<TickProbeComponent>();
    GameObject* PlayObjectSecond = Session.GetPlayWorld()->FindByName("Mover");
    PlayObjectSecond->AddExistingComponent(Probe, false);
    Session.Pause();
    Expect(Session.GetState() == PlaySessionState::Paused, "Paused");
    Session.StepFrame(0.016f);
    Expect(Probe->TickCount == 1, "StepFrame while paused");
    BoundEngine.Tick(0.016f);
    Expect(Probe->TickCount == 1, "Engine tick does not advance while paused");
    Session.Resume();
    BoundEngine.Tick(0.016f);
    Expect(Probe->TickCount == 2, "Engine tick advances while playing");
    Session.StopPlay();

    Expect(Session.StartPlay(), "Third StartPlay cycle");
    Session.StopPlay();
    Expect(Session.StartPlay(), "Fourth StartPlay cycle");
    Session.StopPlay();
    Expect(NearlyEqual(EditObjectAfterStop->GetTransform().Position.x, 1.f), "Edit still unchanged after cycles");

    TickProbeComponent* StandaloneProbe = Memory->NewObject<TickProbeComponent>();
    EditObjectAfterStop->AddExistingComponent(StandaloneProbe, false);
    BoundEngine.Tick(0.016f);
    Expect(StandaloneProbe->TickCount == 0, "Edit world does not tick outside play");
    Expect(BoundEngine.StartGame(), "Standalone simulation starts without PlaySession");
    BoundEngine.Tick(0.016f);
    Expect(StandaloneProbe->TickCount == 1, "Standalone components tick exactly once");
    Expect(!Session.StartPlay(), "Editor play cannot overlap standalone simulation");
    BoundEngine.StopGame();
    BoundEngine.Tick(0.016f);
    Expect(StandaloneProbe->TickCount == 1, "Stopping standalone freezes components");
    const ObjectHandle PreviousWorld = EditWorld->GetObjectHandle();
    BoundEngine.AdoptScene({});
    Expect(Memory->ResolveHandle(PreviousWorld) == nullptr, "Engine destroys its owned scene");
    BoundEngine.Shutdown();

    if (Failures > 0)
    {
        std::cout << Failures << " failure(s)\n" << std::flush;
        return 1;
    }

    std::cout << "All PlaySession tests passed\n" << std::flush;
    return 0;
}
