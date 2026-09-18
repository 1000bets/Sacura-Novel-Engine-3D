#pragma once

#include "Reflection/TypeId.h"

#include <cstdint>
#include <string>
#include <vector>

struct EnumValueDescriptor
{
    std::string Name;
    int64_t Value = 0;
};

struct EnumDescriptor
{
    TypeId Id{};
    uint32_t SchemaVersion = 1;
    std::vector<EnumValueDescriptor> Values;
};
