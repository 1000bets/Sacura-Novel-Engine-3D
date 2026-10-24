#pragma once

#include "Assets/Resources/StaticMeshResource.h"
#include "Assets/Resources/SkeletalMeshResource.h"
#include "Assets/Resources/SkeletonResource.h"
#include "Assets/Resources/AnimationClipResource.h"
#include <memory>
#include <cstdint>
#include <vector>

std::shared_ptr<const StaticMeshResource> BuildStandardCharacterPose(const std::string& Pose, float Time);
std::shared_ptr<const StaticMeshResource> BuildSkinnedCharacterPose(const SkeletalMeshResource& Mesh,
    const SkeletonResource& Skeleton, const AnimationClipResource* Animation, float Ratio, bool bInPlace, std::string& Error);

class CharacterPoseCache
{
public:
    std::shared_ptr<const StaticMeshResource> BuildPose(std::shared_ptr<const SkeletalMeshResource> Model,
        std::shared_ptr<const SkeletonResource> Skeleton, std::shared_ptr<const AnimationClipResource> Animation,
        float Ratio, bool bInPlace, std::string& Error);

private:
    struct Entry
    {
        std::shared_ptr<const SkeletalMeshResource> Model;
        std::shared_ptr<const SkeletonResource> Skeleton;
        std::shared_ptr<const AnimationClipResource> Animation;
        float Ratio = 0.f;
        bool bInPlace = false;
        std::shared_ptr<const StaticMeshResource> Pose;
    };
    uint64_t FrameIndex = UINT64_MAX;
    std::vector<Entry> Entries;
};
