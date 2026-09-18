#pragma once

#include "Platform/NativeWindowInfo.h"
#include "Platform/GraphicsBackend.h"
#include "Rendering/RenderDevice.h"
#include "Rendering/Resources/GpuBuffer.h"
#include "Rendering/Resources/RenderResourceManager.h"
#include "Rendering/RenderResourceHandles.h"

#include <cstdint>
#include <memory>
#include <string>

struct RenderFrameData;

namespace Diligent
{
struct IPipelineState;
struct IShaderResourceBinding;
struct IShader;
}

class Renderer
{
public:
    void Initialize(const NativeWindowInfo& WindowInfo, GraphicsBackend BackendPreference, const std::string& ShaderDirectory);
    void Render(const RenderFrameData& Frame);
    void Resize(uint32_t Width, uint32_t Height);
    void Shutdown();

    MeshHandle GetDefaultMesh() const { return DefaultMesh; }
    bool IsInitialized() const { return bInitialized; }

    RenderResourceManager& GetResources() { return Resources; }
    const RenderResourceManager& GetResources() const { return Resources; }

private:
    bool CreateShaders(const std::string& ShaderDirectory);
    bool CreatePipeline();
    bool CreateConstantBuffers();
    void CreateDefaultMesh();

    bool bInitialized = false;
    RenderDevice Device;
    RenderResourceManager Resources;
    MeshHandle DefaultMesh;

    GpuBuffer FrameConstantBuffer;
    GpuBuffer ObjectConstantBuffer;

    struct PipelineState;
    PipelineState* Pipeline = nullptr;
};
