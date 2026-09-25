#pragma once

#include "Platform/NativeWindowInfo.h"
#include "Rendering/RenderView.h"

#include <cstdint>

namespace Diligent
{
struct ISwapChain;
}

class RenderDevice;

class RenderSurface
{
public:
    RenderSurface() = default;

    RenderSurfaceId GetId() const { return Id; }
    const NativeWindowInfo& GetWindowInfo() const { return WindowInfo; }
    Diligent::ISwapChain* GetSwapChain() const { return SwapChain; }
    uint32_t GetWidth() const { return WindowInfo.Width; }
    uint32_t GetHeight() const { return WindowInfo.Height; }
    bool IsValid() const { return Id.IsValid() && SwapChain != nullptr; }

private:
    friend class RenderDevice;

    RenderSurfaceId Id{};
    NativeWindowInfo WindowInfo{};
    Diligent::ISwapChain* SwapChain = nullptr;
};
