#pragma once

#include "Core/Types.h"

#include <cstdint>

struct ObjectHandle
{
    ObjectID Id = INVALID_OBJECT_ID;
    uint32_t Generation = 0;

    bool IsValid() const
    {
        return Id != INVALID_OBJECT_ID && Generation != 0;
    }

    bool operator==(const ObjectHandle& Other) const
    {
        return Id == Other.Id && Generation == Other.Generation;
    }

    bool operator!=(const ObjectHandle& Other) const
    {
        return !(*this == Other);
    }
};

struct ScriptInstanceHandle
{
    uint64_t Value = 0;

    bool IsValid() const
    {
        return Value != 0;
    }

    bool operator==(const ScriptInstanceHandle& Other) const
    {
        return Value == Other.Value;
    }

    bool operator!=(const ScriptInstanceHandle& Other) const
    {
        return Value != Other.Value;
    }
};
