#include "Core/IO/ImageBuffer.h"
#include <stb_image_write.h>
#include <algorithm>
#include <cmath>
#include <cstring>

ImageBuffer ImageBuffer::FromRgba(const uint8_t* Source, int Width, int Height, int RowStride, bool bPremultiplied)
{
    if (!Source || Width <= 0 || Height <= 0 || Width > 8192 || Height > 8192 || RowStride < Width * 4)
    {
        return {};
    }
    ImageBuffer Result{Width, Height, std::vector<uint8_t>(static_cast<size_t>(Width) * Height * 4)};
    for (int Row = 0; Row < Height; ++Row)
    {
        std::memcpy(Result.Pixels.data() + static_cast<size_t>(Row) * Width * 4,
            Source + static_cast<size_t>(Row) * RowStride, static_cast<size_t>(Width) * 4);
    }
    if (bPremultiplied)
    {
        for (size_t Offset = 0; Offset < Result.Pixels.size(); Offset += 4)
        {
            const unsigned Alpha = Result.Pixels[Offset + 3];
            for (size_t Channel = 0; Channel < 3; ++Channel)
            {
                if (Alpha == 0)
                {
                    Result.Pixels[Offset + Channel] = 0;
                }
                else
                {
                    Result.Pixels[Offset + Channel] = static_cast<uint8_t>(
                        std::min(255u, (Result.Pixels[Offset + Channel] * 255u + Alpha / 2) / Alpha));
                }
            }
        }
    }
    return Result;
}

ImageBuffer ImageBuffer::ResizeToFit(int MaximumWidth, int MaximumHeight) const
{
    if (Pixels.empty() || Width <= 0 || Height <= 0 || MaximumWidth <= 0 || MaximumHeight <= 0)
    {
        return {};
    }
    const double Scale = std::min({1.0, static_cast<double>(MaximumWidth) / Width, static_cast<double>(MaximumHeight) / Height});
    if (Scale == 1.0)
    {
        return *this;
    }
    ImageBuffer Result;
    Result.Width = std::max(1, static_cast<int>(std::round(Width * Scale)));
    Result.Height = std::max(1, static_cast<int>(std::round(Height * Scale)));
    Result.Pixels.resize(static_cast<size_t>(Result.Width) * Result.Height * 4);
    for (int Row = 0; Row < Result.Height; ++Row)
    {
        for (int Column = 0; Column < Result.Width; ++Column)
        {
            const double SourceHorizontal = std::clamp((Column + 0.5) * Width / Result.Width - 0.5, 0.0, static_cast<double>(Width - 1));
            const double SourceVertical = std::clamp((Row + 0.5) * Height / Result.Height - 0.5, 0.0, static_cast<double>(Height - 1));
            const int Left = static_cast<int>(SourceHorizontal);
            const int Top = static_cast<int>(SourceVertical);
            const double HorizontalWeight = SourceHorizontal - Left;
            const double VerticalWeight = SourceVertical - Top;
            const size_t Offsets[] = {
                (static_cast<size_t>(Top) * Width + Left) * 4,
                (static_cast<size_t>(Top) * Width + std::min(Left + 1, Width - 1)) * 4,
                (static_cast<size_t>(std::min(Top + 1, Height - 1)) * Width + Left) * 4,
                (static_cast<size_t>(std::min(Top + 1, Height - 1)) * Width + std::min(Left + 1, Width - 1)) * 4};
            const double Weights[] = {(1 - HorizontalWeight) * (1 - VerticalWeight), HorizontalWeight * (1 - VerticalWeight),
                (1 - HorizontalWeight) * VerticalWeight, HorizontalWeight * VerticalWeight};
            double Alpha = 0;
            double Channels[3]{};
            for (size_t Sample = 0; Sample < 4; ++Sample)
            {
                const double WeightedAlpha = Pixels[Offsets[Sample] + 3] * Weights[Sample];
                Alpha += WeightedAlpha;
                for (size_t Channel = 0; Channel < 3; ++Channel)
                {
                    Channels[Channel] += Pixels[Offsets[Sample] + Channel] * WeightedAlpha;
                }
            }
            const size_t Destination = (static_cast<size_t>(Row) * Result.Width + Column) * 4;
            Result.Pixels[Destination + 3] = static_cast<uint8_t>(std::clamp(std::round(Alpha), 0.0, 255.0));
            if (Alpha > 0)
            {
                for (size_t Channel = 0; Channel < 3; ++Channel)
                {
                    Result.Pixels[Destination + Channel] = static_cast<uint8_t>(std::clamp(std::round(Channels[Channel] / Alpha), 0.0, 255.0));
                }
            }
        }
    }
    return Result;
}

std::vector<uint8_t> ImageBuffer::EncodePng() const
{
    std::vector<uint8_t> Result;
    if (Width <= 0 || Height <= 0 || Width > 8192 || Height > 8192
        || Pixels.size() != static_cast<size_t>(Width) * Height * 4)
    {
        return Result;
    }
    const auto Append = [](void* Context, void* Data, int Size)
    {
        auto& Bytes = *static_cast<std::vector<uint8_t>*>(Context);
        const auto* First = static_cast<const uint8_t*>(Data);
        Bytes.insert(Bytes.end(), First, First + Size);
    };
    if (!stbi_write_png_to_func(Append, &Result, Width, Height, 4, Pixels.data(), Width * 4))
    {
        Result.clear();
    }
    return Result;
}
