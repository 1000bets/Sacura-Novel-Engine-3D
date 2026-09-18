#pragma once

#include "Reflection/PropertyFlags.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

class Object;

struct PropertyDescriptor
{
    TypeId DeclaringTypeId{};
    PropertyId Id{};
    TypeId ValueTypeId{};
    PropertyAttributes Attributes{};
    bool bReadOnly = false;
    bool bIsField = false;

    using ReadObjectFunction = ReflectionDiagnostic (*)(
        Object* Instance,
        const PropertyDescriptor& Descriptor,
        ReflectedValue& OutValue);
    using WriteObjectFunction = ReflectionDiagnostic (*)(
        Object* Instance,
        const PropertyDescriptor& Descriptor,
        const ReflectedValue& InValue);
    using ReadRawFunction = ReflectionDiagnostic (*)(void* Instance, ReflectedValue& OutValue);
    using WriteRawFunction = ReflectionDiagnostic (*)(void* Instance, const ReflectedValue& InValue);

    ReadObjectFunction ReadObject = nullptr;
    WriteObjectFunction WriteObject = nullptr;
    ReadRawFunction ReadRaw = nullptr;
    WriteRawFunction WriteRaw = nullptr;
};
