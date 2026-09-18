#pragma once

#include "Assets/Resources/SkinBinding.h"
#include "Assets/Resources/StaticMeshResource.h"

#include <memory>

struct SkeletalMeshResource
{
    StaticMeshResource Mesh{};
    std::shared_ptr<const SkinBinding> Binding;
};
