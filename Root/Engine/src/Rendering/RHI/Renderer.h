#pragma once

#include "Platform/NativeWindowInfo.h"
#include "Platform/GraphicsBackend.h"
#include "Rendering/RHI/RenderDevice.h"
#include "Rendering/RHI/GpuBuffer.h"
#include "Rendering/RHI/RenderResourceManager.h"
#include "Rendering/RenderResourceHandles.h"
#include "Rendering/ImGuiOverlaySnapshot.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include "Rendering/RenderSettings.h"

struct RenderFrameData;
struct RenderViewFrame;
struct ImGuiContext;

namespace Diligent
{
struct IPipelineState;
struct IShaderResourceBinding;
struct IShader;
struct IDeviceContext;
struct ISwapChain;
class ImGuiDiligentRenderer;
}

class Renderer
{
public:
    bool Initialize(const NativeWindowInfo& WindowInfo, GraphicsBackend BackendPreference, const std::string& ShaderDirectory);
    void Render(const RenderFrameData& Frame);
    void Resize(uint32_t Width, uint32_t Height);
    void Shutdown();
    bool AddSurface(RenderSurfaceId Surface, const NativeWindowInfo& WindowInfo);
    void RemoveSurface(RenderSurfaceId Surface);
    void ResizeSurface(RenderSurfaceId Surface, uint32_t Width, uint32_t Height);
    std::unordered_map<uint32_t, RenderStatistics> GetStatistics() const;

    MeshHandle GetDefaultMesh() const { return DefaultMesh; }
    bool IsInitialized() const { return bInitialized; }

    RenderResourceManager& GetResources() { return Resources; }
    const RenderResourceManager& GetResources() const { return Resources; }

private:
    bool CreatePipeline(const std::string& ShaderDirectory);
    bool CreateSurfaceResources(RenderSurfaceId Surface);
    void RenderView(const RenderViewFrame& View, uint64_t FrameIndex);
    void CreateImGuiOverlay();
    void DestroyImGuiOverlay();
    void RenderImGuiOverlay(Diligent::IDeviceContext* Context, Diligent::ISwapChain* SwapChain, const ImGuiOverlaySnapshot& Overlay);

    bool bInitialized = false;
    RenderDevice Device;
    RenderResourceManager Resources;
    MeshHandle DefaultMesh;

    struct PipelineState;
    PipelineState* Pipeline = nullptr;
    ImGuiContext* OverlayImGuiContext = nullptr;
    Diligent::ImGuiDiligentRenderer* OverlayImGuiRenderer = nullptr;
};
