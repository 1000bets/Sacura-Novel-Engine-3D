#pragma once

#include "Assets/AssetTypes.h"

#include <cstdint>
#include <string>

struct ContentHash
{
    uint64_t Value = 0;

    bool IsValid() const { return Value != 0; }
    std::string ToHex() const;

    bool operator==(const ContentHash& Other) const { return Value == Other.Value; }
    bool operator!=(const ContentHash& Other) const { return Value != Other.Value; }

    // FNV-1a 64-bit over raw bytes. Stable across runs; not cryptographic.
    static ContentHash FromBytes(const void* Data, size_t ByteCount);
    static bool TryHashFile(const std::string& AbsolutePath, ContentHash& OutHash, AssetDiagnostic& OutError);
};
