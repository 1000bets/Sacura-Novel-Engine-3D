#pragma once

#include "Platform/NativeWindowInfo.h"
#include "Platform/GraphicsBackend.h"
#include "Rendering/RHI/RenderDevice.h"
#include "Rendering/RHI/GpuBuffer.h"
#include "Rendering/RHI/RenderResourceManager.h"
#include "Rendering/RenderResourceHandles.h"
#include "Rendering/ImGuiOverlaySnapshot.h"
#include "Materials/MaterialDefinition.h"
#include "Materials/MaterialCompilation.h"
#include "Materials/MaterialImageRequest.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include "Rendering/RenderSettings.h"

struct RenderFrameData;
struct RenderViewFrame;
struct ImGuiContext;
class SharedFrameExporter;
class SharedGpuFrameStream;

namespace Diligent
{
struct IPipelineState;
struct IShaderResourceBinding;
struct IShader;
struct IDeviceContext;
struct ITextureView;
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
    void ResizeSurface(RenderSurfaceId Surface, uint32_t Width, uint32_t Height, bool bInteractive = false);
    std::unordered_map<uint32_t, RenderStatistics> GetStatistics() const;
    void PrepareMaterial(const ResolvedMaterial& Material, std::shared_ptr<MaterialCompilation> Compilation);
    void QueueMaterialImage(std::shared_ptr<MaterialImageRequest> Request);
    void PollMaterialImages();
    void ClearMaterialResources();
    void SetSharedFrameStream(std::shared_ptr<SharedGpuFrameStream> Stream);
    void PollSharedFrames();

    MeshHandle GetDefaultMesh() const { return DefaultMesh; }
    bool IsInitialized() const { return bInitialized; }

    RenderResourceManager& GetResources() { return Resources; }
    const RenderResourceManager& GetResources() const { return Resources; }

private:
    std::string GetMaterialVariantKey(const std::string& MaterialKey) const;
    bool CreatePipeline(const std::string& ShaderDirectory);
    void UpdateMaterialCompilations();
    bool CreateSurfaceResources(RenderSurfaceId Surface);
    void RenderView(const RenderViewFrame& View, uint64_t FrameIndex, Diligent::ITextureView* Output = nullptr);
    void RenderMaterialPreview(const MaterialImageRequest& Request, Diligent::ITextureView* Output);
    void CreateImGuiOverlay();
    void DestroyImGuiOverlay();
    void RenderImGuiOverlay(Diligent::IDeviceContext* Context, Diligent::ISwapChain* SwapChain, const ImGuiOverlaySnapshot& Overlay);

    bool bInitialized = false;
    RenderDevice Device;
    RenderResourceManager Resources;
    MeshHandle DefaultMesh;
    MeshHandle PreviewMeshes[3];
    struct PosedMeshEntry
    {
        std::shared_ptr<const StaticMeshResource> Source;
        MeshHandle Mesh;
        uint64_t LastFrame = 0;
    };
    std::unordered_map<const StaticMeshResource*, PosedMeshEntry> PosedMeshes;

    struct PipelineState;
    PipelineState* Pipeline = nullptr;
    ImGuiContext* OverlayImGuiContext = nullptr;
    std::shared_ptr<SharedFrameExporter> FrameExporter;
    Diligent::ImGuiDiligentRenderer* OverlayImGuiRenderer = nullptr;
};
