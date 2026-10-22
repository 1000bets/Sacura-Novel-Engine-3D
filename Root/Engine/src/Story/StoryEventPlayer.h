#pragma once
#include "Story/StoryTypes.h"
#include "Core/Object/ObjectHandle.h"
#include <memory>

// Staging orchestrates event flows; Authoring::Runtime remains the task executor.
class StoryEventPlayer
{
  public:
    StoryEventPlayer(ObjectHandle World, uint64_t& NextInstanceId);
    ~StoryEventPlayer();
    void BeginPhase(const StoryDocument& Document, const StoryNode& Node, StoryEventPhase Phase);
    void Tick(float DeltaSeconds);
    bool IsReady() const;
    const std::string& GetError() const;
    std::vector<StoryEventSnapshot> GetSnapshots() const;
    bool SetPaused(uint64_t InstanceId, bool bPaused);
    bool Stop(uint64_t InstanceId);

  private:
    struct Implementation;
    std::unique_ptr<Implementation> State;
};
