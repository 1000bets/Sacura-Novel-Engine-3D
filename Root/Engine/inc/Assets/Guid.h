#pragma once

#include <array>
#include <cstdint>
#include <string>

struct Guid
{
    std::array<uint8_t, 16> Bytes{};

    bool IsValid() const;
    std::string ToString() const;

    static Guid Generate();
    static bool TryParse(const std::string& Text, Guid& OutGuid);

    bool operator==(const Guid& Other) const;
    bool operator!=(const Guid& Other) const;
};

struct GuidHash
{
    size_t operator()(const Guid& Value) const;
};

using AssetId = Guid;
using SubAssetId = Guid;
