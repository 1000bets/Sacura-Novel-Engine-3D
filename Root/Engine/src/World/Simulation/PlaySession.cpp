#include "World/Simulation/PlaySession.h"

#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"

#include <string>
#include "Engine.h"
#include "World/Scene.h"
#include "World/Serialization/SceneSerializer.h"
#include "Physics/PhysicsWorld.h"
#include "Navigation/NavigationWorld.h"

PlaySession::PlaySession() = default;
PlaySession::~PlaySession() = default;

void PlaySession::BindEngine(Engine* InEngine)
{
    BoundEngine = InEngine;
}

bool PlaySession::StartPlay()
{
    AssertGameThread();
    if (BoundEngine == nullptr)
    {
        return false;
    }
    if (State != PlaySessionState::Stopped || BoundEngine->IsGameRunning())
    {
        return false;
    }

    Scene* SourceEditWorld = BoundEngine->GetActiveScene();
    if (SourceEditWorld == nullptr)
    {
        PrintString("PlaySession: StartPlay failed — no active scene");
        return false;
    }

    if (!BoundEngine->PrepareSceneMaterials(*SourceEditWorld)
        || !BoundEngine->PrepareSceneSimulation(*SourceEditWorld))
    {
        return false;
    }
    BoundEngine->MaterialTime = 0.f;
    EditWorld = SourceEditWorld;
    EditSceneJson.clear();
    const SceneSerializeResult Serialized = SceneSerializer::SerializeToJson(*EditWorld, EditSceneJson);
    if (!Serialized.bOk)
    {
        EditWorld = nullptr;
        PrintString(std::string("PlaySession: StartPlay serialize failed: ") + Serialized.Error);
        return false;
    }

    Scene* LoadedPlayWorld = nullptr;
    const SceneSerializeResult Deserialized = SceneSerializer::DeserializeFromJson(EditSceneJson, LoadedPlayWorld);
    if (!Deserialized.bOk || LoadedPlayWorld == nullptr)
    {
        EditWorld = nullptr;
        EditSceneJson.clear();
        PrintString(std::string("PlaySession: StartPlay deserialize failed: ") + Deserialized.Error);
        return false;
    }

    PlayWorld.reset(LoadedPlayWorld);
    PlayWorld->SetAssetManager(&BoundEngine->GetAssetManager());
    PlayWorld->SetJobSystem(&BoundEngine->GetJobSystem());
    PlayWorld->SetAuthoringOperations(BoundEngine->AuthoringOperations);
    PlayWorld->GetNavigation().CopyMeshesFrom(SourceEditWorld->GetNavigation());
    if (!BoundEngine->BeginInputSession())
    {
        DestroyPlayWorld();
        EditWorld = nullptr;
        EditSceneJson.clear();
        return false;
    }
    State = PlaySessionState::Playing;
    if (!PlayWorld->BeginPlay())
    {
        PrintString("PlaySession: physics initialization failed: " + PlayWorld->GetPhysics().GetDiagnostic());
        DestroyPlayWorld();
        EditWorld = nullptr;
        EditSceneJson.clear();
        State = PlaySessionState::Stopped;
        BoundEngine->PlayerInput.reset();
        return false;
    }
    BoundEngine->BeginStory(PlayWorld.get());
    PrintString("PlaySession: started");
    return true;
}

void PlaySession::DestroyPlayWorld()
{
    PlayWorld.reset();
}

void PlaySession::StopPlay()
{
    AssertGameThread();
    if (State == PlaySessionState::Stopped)
    {
        return;
    }

    BoundEngine->StopGame();
    PlayWorld->EndPlay();
    DestroyPlayWorld();
    EditWorld = nullptr;

    EditSceneJson.clear();
    State = PlaySessionState::Stopped;
    PrintString("PlaySession: stopped");
}

void PlaySession::Pause()
{
    if (State == PlaySessionState::Playing)
    {
        State = PlaySessionState::Paused;
        BoundEngine->GetInputSession()->SetPaused(true);
        PrintString("PlaySession: paused");
    }
}

void PlaySession::Resume()
{
    if (State == PlaySessionState::Paused)
    {
        State = PlaySessionState::Playing;
        BoundEngine->GetInputSession()->SetPaused(false);
        PrintString("PlaySession: resumed");
    }
}

void PlaySession::StepFrame(float Delta)
{
    AssertGameThread();
    if (State != PlaySessionState::Paused || PlayWorld == nullptr)
    {
        return;
    }
    BoundEngine->TickWorld(*PlayWorld, Delta, true);
}

void PlaySession::Tick(float Delta)
{
    AssertGameThread();
    if (State != PlaySessionState::Playing || PlayWorld == nullptr)
    {
        return;
    }
    BoundEngine->TickWorld(*PlayWorld, Delta);
}
