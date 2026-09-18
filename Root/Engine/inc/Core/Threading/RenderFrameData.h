#pragma once

#include "Rendering/RenderScene.h"

#include <cstdint>

struct RenderFrameData
{
    uint64_t FrameIndex = 0;
    RenderScene Scene;
};
