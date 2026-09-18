#pragma once

#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <cstddef>

struct TypeDescriptor
{
    TypeKind Kind = TypeKind::Unknown;
    TypeId Id{};
    uint32_t SchemaVersion = 1;
    size_t ByteSize = 0;

    using ConstructFunction = void (*)(void* Destination);
    using DestroyFunction = void (*)(void* Destination);
    using CopyFunction = void (*)(void* Destination, const void* Source);
    using EqualsFunction = bool (*)(const void* Left, const void* Right);
    using ToReflectedFunction = ReflectionDiagnostic (*)(const void* Source, ReflectedValue& OutValue);
    using FromReflectedFunction = ReflectionDiagnostic (*)(void* Destination, const ReflectedValue& InValue);
    using ValidateFunction = ReflectionDiagnostic (*)(const ReflectedValue& InValue, const PropertyAttributes* Attributes);

    ConstructFunction Construct = nullptr;
    DestroyFunction Destroy = nullptr;
    CopyFunction Copy = nullptr;
    EqualsFunction Equals = nullptr;
    ToReflectedFunction ToReflected = nullptr;
    FromReflectedFunction FromReflected = nullptr;
    ValidateFunction Validate = nullptr;
};
