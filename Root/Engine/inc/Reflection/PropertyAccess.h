#pragma once

#include "Reflection/PropertyDescriptor.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <cstdint>

class Object;
class Class;

enum class PropertyAccessContext : uint8_t
{
    Default = 0,
    Inspector,
    Deserialize,
    Script
};

struct PropertyAccess
{
    static ReflectionDiagnostic GetProperty(
        Object* Instance,
        const PropertyId& Property,
        ReflectedValue& OutValue,
        PropertyAccessContext Context = PropertyAccessContext::Default);

    static ReflectionDiagnostic SetProperty(
        Object* Instance,
        const PropertyId& Property,
        const ReflectedValue& InValue,
        PropertyAccessContext Context = PropertyAccessContext::Default);

    static ReflectionDiagnostic GetProperty(
        void* Instance,
        const TypeId& DeclaringTypeId,
        const PropertyId& Property,
        ReflectedValue& OutValue,
        PropertyAccessContext Context = PropertyAccessContext::Default);

    static ReflectionDiagnostic SetProperty(
        void* Instance,
        const TypeId& DeclaringTypeId,
        const PropertyId& Property,
        const ReflectedValue& InValue,
        PropertyAccessContext Context = PropertyAccessContext::Default);

    static ReflectionDiagnostic GetProperty(
        Object* Instance,
        const PropertyDescriptor& Descriptor,
        ReflectedValue& OutValue,
        PropertyAccessContext Context = PropertyAccessContext::Default);

    static ReflectionDiagnostic SetProperty(
        Object* Instance,
        const PropertyDescriptor& Descriptor,
        const ReflectedValue& InValue,
        PropertyAccessContext Context = PropertyAccessContext::Default);

    static ReflectionDiagnostic CopyPropertiesFrom(
        Object* Source,
        Object* Destination,
        PropertyAccessContext Context = PropertyAccessContext::Deserialize);

    static ReflectionDiagnostic ResetToClassDefaults(
        Object* Instance,
        PropertyAccessContext Context = PropertyAccessContext::Deserialize);
};
