#pragma once

#include <cstdint>

struct NativeWindowInfo
{
    void* WindowHandle = nullptr;
    uint32_t Width = 0;
    uint32_t Height = 0;
};
