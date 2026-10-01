#pragma once

#include "Rendering/ImGuiOverlaySnapshot.h"
#include "Rendering/RenderScene.h"
#include "Rendering/RenderSettings.h"
#include "Rendering/RenderView.h"

#include <cstdint>

struct RenderViewFrame
{
    RenderSurfaceId Surface{1};
    RenderScene Scene;
    RenderSettings Settings;
    float TimeSeconds = 0.f;
    ImGuiOverlaySnapshot Overlay;
};

struct RenderFrameData
{
    uint64_t FrameIndex = 0;
    std::vector<RenderViewFrame> Views;
};
