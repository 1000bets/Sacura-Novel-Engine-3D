#pragma once

#include "Rendering/RenderScene.h"
#include "Rendering/RenderSettings.h"
#include "Rendering/RenderView.h"

#include <cstdint>

struct RenderViewFrame
{
    RenderSurfaceId Surface{1};
    RenderScene Scene;
    RenderSettings Settings;
};

struct RenderFrameData
{
    uint64_t FrameIndex = 0;
    RenderScene Scene;
    std::vector<RenderViewFrame> Views;
};
