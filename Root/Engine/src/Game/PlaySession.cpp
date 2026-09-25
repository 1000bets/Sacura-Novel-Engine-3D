#include "Game/PlaySession.h"

#include "Core/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"

#include <string>
#include "Engine.h"
#include "Game/Scene.h"
#include "Game/SceneSerializer.h"

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
    State = PlaySessionState::Playing;
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
        PrintString("PlaySession: paused");
    }
}

void PlaySession::Resume()
{
    if (State == PlaySessionState::Paused)
    {
        State = PlaySessionState::Playing;
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
    BoundEngine->TickWorld(*PlayWorld, Delta);
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
