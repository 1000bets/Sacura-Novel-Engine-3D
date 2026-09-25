#pragma once

#include <cstdint>

namespace Diligent
{
struct IDeviceContext;
}

struct RenderContext
{
    uint64_t FrameIndex = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
    Diligent::IDeviceContext* DeviceContext = nullptr;
};
