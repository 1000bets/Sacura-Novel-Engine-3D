#pragma once

#include <cstdint>
#include <vector>

struct ImageBuffer
{
    int Width = 0;
    int Height = 0;
    std::vector<uint8_t> Pixels;

    static ImageBuffer FromRgba(const uint8_t* Source, int Width, int Height, int RowStride, bool bPremultiplied = false);
    ImageBuffer ResizeToFit(int MaximumWidth, int MaximumHeight) const;
    std::vector<uint8_t> EncodePng() const;
};
