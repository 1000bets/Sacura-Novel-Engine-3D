#pragma once

#include "Assets/AssetTypes.h"

#include <type_traits>

template <typename ResourceType>
struct AssetRef
{
    AssetKey Key{};

    bool IsValid() const { return Key.IsValid(); }

    bool operator==(const AssetRef& Other) const { return Key == Other.Key; }
    bool operator!=(const AssetRef& Other) const { return Key != Other.Key; }
};

static_assert(std::is_trivially_copyable_v<AssetId>, "AssetId must stay trivially copyable");
