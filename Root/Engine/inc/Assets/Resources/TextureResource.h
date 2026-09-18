#pragma once

#include <cstdint>
#include <vector>

enum class TextureColorSpace
{
    Linear = 0,
    Srgb = 1
};

struct TextureResource
{
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t ChannelCount = 0;
    uint32_t RowStride = 0;
    TextureColorSpace ColorSpace = TextureColorSpace::Srgb;
    std::vector<uint8_t> Pixels;
};
