#pragma once

#include <cstdint>

struct MeshHandle
{
    uint64_t Value = 0;

    bool IsValid() const { return Value != 0; }

    bool operator==(const MeshHandle& Other) const { return Value == Other.Value; }
    bool operator!=(const MeshHandle& Other) const { return Value != Other.Value; }
};

struct MaterialHandle
{
    uint64_t Value = 0;

    bool IsValid() const { return Value != 0; }

    bool operator==(const MaterialHandle& Other) const { return Value == Other.Value; }
    bool operator!=(const MaterialHandle& Other) const { return Value != Other.Value; }
};

struct TextureHandle
{
    uint64_t Value = 0;

    bool IsValid() const { return Value != 0; }

    bool operator==(const TextureHandle& Other) const { return Value == Other.Value; }
    bool operator!=(const TextureHandle& Other) const { return Value != Other.Value; }
};
