#pragma once

#include "ozz/animation/runtime/animation.h"

#include <memory>
#include <string>

struct AnimationClipResource
{
    std::string Name;
    std::shared_ptr<const ozz::animation::Animation> Animation;
};
