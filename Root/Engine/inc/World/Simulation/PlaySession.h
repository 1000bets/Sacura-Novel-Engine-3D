#pragma once

#include <string>
#include <memory>

class Engine;
class Scene;

enum class PlaySessionState
{
    Stopped,
    Playing,
    Paused
};

class PlaySession
{
public:
    PlaySession();
    ~PlaySession();
    void BindEngine(Engine* InEngine);

    bool StartPlay();
    void StopPlay();
    void Pause();
    void Resume();
    void StepFrame(float Delta);
    void Tick(float Delta);

    PlaySessionState GetState() const { return State; }
    Scene* GetEditWorld() const { return EditWorld; }
    Scene* GetPlayWorld() const { return PlayWorld.get(); }
    bool IsSimulating() const { return State != PlaySessionState::Stopped; }

private:
    void DestroyPlayWorld();

    Engine* BoundEngine = nullptr;
    Scene* EditWorld = nullptr;
    std::unique_ptr<Scene> PlayWorld;
    std::string EditSceneJson;
    PlaySessionState State = PlaySessionState::Stopped;
};
