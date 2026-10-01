#pragma once

#include "Platform/NativeWindowInfo.h"
#include "Rendering/Frame/RenderFrameData.h"
#include "Rendering/SceneExtractor.h"

#include <memory>
#include <unordered_map>

class Scene;

class PresentationController
{
public:
    SceneExtractor& GetSceneExtractor() { return Extractor; }
    bool Contains(RenderSurfaceId Surface, const NativeWindowInfo& Window) const;
    void Register(RenderSurfaceId Surface, const NativeWindowInfo& Window);
    bool Unregister(RenderSurfaceId Surface);
    void Resize(RenderSurfaceId Surface, uint32_t Width, uint32_t Height);
    void Configure(RenderSurfaceId Surface, bool bEditScene, const RenderCamera& Camera, const RenderSettings& Settings);
    void SetOverlay(RenderSurfaceId Surface, ImGuiOverlaySnapshot Overlay);
    void Clear();
    std::unique_ptr<RenderFrameData> BuildFrame(uint64_t FrameIndex, Scene* ActiveWorld, Scene* EditWorld, float TimeSeconds = 0.f);

private:
    struct PresentationState
    {
        NativeWindowInfo Window;
        RenderCamera Camera;
        RenderSettings Settings;
        ImGuiOverlaySnapshot Overlay;
        bool bEditScene = false;
    };

    SceneExtractor Extractor;
    std::unordered_map<uint32_t, PresentationState> Presentations;
};
