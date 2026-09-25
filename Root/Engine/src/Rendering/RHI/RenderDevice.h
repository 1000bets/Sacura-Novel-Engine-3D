#pragma once

#include "Platform/GraphicsBackend.h"
#include "Rendering/RenderView.h"
#include "Platform/NativeWindowInfo.h"

#include <cstdint>
#include <string>

namespace Diligent
{
struct IRenderDevice;
struct IDeviceContext;
struct ISwapChain;
struct IEngineFactory;
}

class RenderDevice
{
public:
    void Initialize(const NativeWindowInfo& WindowInfo, GraphicsBackend BackendPreference);
    void Shutdown();
    bool AddSurface(RenderSurfaceId Surface, const NativeWindowInfo& WindowInfo);
    void RemoveSurface(RenderSurfaceId Surface);
    void ResizeSurface(RenderSurfaceId Surface, uint32_t Width, uint32_t Height);
    Diligent::ISwapChain* GetSurface(RenderSurfaceId Surface) const;
    void WaitForIdle();
    void Resize(uint32_t Width, uint32_t Height);

    Diligent::IRenderDevice* GetDevice() const;
    Diligent::IDeviceContext* GetImmediateContext() const;
    Diligent::ISwapChain* GetSwapChain() const;
    Diligent::IEngineFactory* GetEngineFactory() const;

    GraphicsBackend GetActiveBackend() const { return ActiveBackend; }
    const std::string& GetAdapterName() const { return AdapterName; }
    uint32_t GetWidth() const { return Width; }
    uint32_t GetHeight() const { return Height; }

    bool IsInitialized() const { return bInitialized; }

private:
    GraphicsBackend ResolveBackend(GraphicsBackend Preference) const;
    bool InitializeBackend(GraphicsBackend Backend, const NativeWindowInfo& WindowInfo);

    bool bInitialized = false;
    GraphicsBackend ActiveBackend = GraphicsBackend::Auto;
    std::string AdapterName = "Unknown";
    uint32_t Width = 0;
    uint32_t Height = 0;

    struct DiligentState;
    DiligentState* State = nullptr;
};
