#pragma once
#include "Story/StoryTypes.h"
#include "Core/Object/ObjectHandle.h"
#include <memory>

// Staging orchestrates event flows; Authoring::Runtime remains the task executor.
class StoryEventPlayer
{
  public:
    explicit StoryEventPlayer(ObjectHandle World);
    ~StoryEventPlayer();
    void BeginPhase(const StoryDocument& Document, const StoryNode& Node, StoryEventPhase Phase);
    void Tick(float DeltaSeconds);
    bool IsReady() const;
    const std::string& GetError() const;

  private:
    struct Implementation;
    std::unique_ptr<Implementation> State;
};
