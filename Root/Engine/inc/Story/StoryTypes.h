#pragma once

#include "Core/Transform.h"

#include <string>
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
    CameraCut
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
};

struct StoryDocument
{
    std::string Name;
    StoryNodeId StartNodeId;
    std::vector<StoryNode> Nodes;
};

struct StorySerializeResult
{
    bool bOk = false;
    std::string Error;
};
