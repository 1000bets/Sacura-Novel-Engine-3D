#pragma once

#include "Reflection/PropertyDescriptor.h"

#include <string>
#include <vector>

struct MethodDescriptor
{
    std::string Name;
    std::vector<PropertyDescriptor> Parameters;
    TypeId ResultTypeId{};
};
