#pragma once

#include "Core/Transform.h"

#include <string>
#include <cstdint>
#include <vector>

using StoryNodeId = std::string;

enum class StoryNodeKind
{
    Line = 0,
    Choice,
    Action,
    End
};

enum class StoryActionKind
{
    Wait = 0,
    MoveTo,
    CameraCut,
    NavigateTo
};

enum class StoryActionState
{
    Running = 0,
    Completed,
    Failed,
    Cancelled
};

struct StoryChoice
{
    std::string Label;
    StoryNodeId NextNodeId;
    std::string Id;
};

struct StoryActionDefinition
{
    StoryActionKind Kind = StoryActionKind::Wait;
    float DurationSeconds = 0.0f;
    std::string TargetObjectId;
    std::string TargetObjectName;
    Vector3 TargetPosition = Vector3::Zero;
    Quaternion TargetRotation = Quaternion::Identity;
    bool bHasTargetRotation = false;
    bool bUseTargetObjectTransform = false;
    std::string Id;
};

enum class StoryEventPhase { Before, OnStart, After };
enum class StoryEventJoin { None, Started, FlowEnd, EventEnd };

enum class StoryEventPlaybackState { Running, Paused, Completed, Cancelled };

// Runtime-only identity and copied presentation data; never serialized into a story.
struct StoryEventSnapshot
{
    uint64_t InstanceId = 0;
    std::string Name;
    StoryNodeId NodeId;
    StoryEventPhase Phase = StoryEventPhase::OnStart;
    StoryEventPlaybackState State = StoryEventPlaybackState::Running;
    bool bCanPause = true;
};

struct StoryEventGroup
{
    std::string Id;
    std::string Name;
    // Groups run in order; actions within a group run together, as in the web editor.
    std::vector<StoryActionDefinition> Actions;
};

struct StoryEventDefinition
{
    std::string Id;
    std::string Name;
    std::string Description;
    std::vector<StoryEventGroup> Groups;
};

struct StoryEventBinding
{
    std::string Id;
    std::string EventId;
    StoryEventJoin Join = StoryEventJoin::EventEnd;
    // Complete action values, keyed by stable action ID. Only this placement changes.
    std::vector<StoryActionDefinition> ActionOverrides;
};

struct StoryEventBatch
{
    std::string Id;
    StoryEventPhase Phase = StoryEventPhase::OnStart;
    bool bParallel = false;
    std::vector<StoryEventBinding> Bindings;
};

struct StoryNode
{
    StoryNodeId Id;
    StoryNodeKind Kind = StoryNodeKind::Line;
    std::string Speaker;
    std::string Text;
    std::string Prompt;
    std::vector<StoryChoice> Choices;
    std::vector<StoryActionDefinition> Actions;
    StoryNodeId NextNodeId;
    std::vector<StoryEventBatch> EventBatches;
};

struct StoryDocument
{
    std::string Name;
    StoryNodeId StartNodeId;
    std::vector<StoryNode> Nodes;
    std::vector<StoryEventDefinition> Events;
};

struct StorySerializeResult
{
    bool bOk = false;
    std::string Error;
};
