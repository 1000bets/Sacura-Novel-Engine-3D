#pragma once

#include "ozz/animation/runtime/animation.h"

#include <memory>
#include <string>
#include <vector>

struct AnimationClipResource
{
    std::string Name;
    std::vector<std::string> JointNames;
    std::shared_ptr<const ozz::animation::Animation> Animation;
};
