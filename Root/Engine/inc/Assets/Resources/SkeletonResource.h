#pragma once

#include "ozz/animation/runtime/skeleton.h"

#include <memory>

struct SkeletonResource
{
    std::shared_ptr<const ozz::animation::Skeleton> Skeleton;
};
