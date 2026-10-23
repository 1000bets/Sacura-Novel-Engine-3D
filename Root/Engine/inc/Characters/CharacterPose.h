#pragma once

#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/SkeletalMeshResource.h"
#include "Assets/Resources/SkeletonResource.h"
#include "Assets/Resources/AnimationClipResource.h"
#include <memory>

std::shared_ptr<const StaticMeshResource> BuildStandardCharacterPose(const std::string& Pose, float Time);
std::shared_ptr<const StaticMeshResource> BuildSkinnedCharacterPose(const SkeletalMeshResource& Mesh,
    const SkeletonResource& Skeleton, const AnimationClipResource* Animation, float Ratio, bool bInPlace, std::string& Error);
