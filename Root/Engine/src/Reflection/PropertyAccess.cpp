#include "Reflection/PropertyAccess.h"

#include "Gameplay/Object.h"
#include "Reflection/Class.h"
#include "Reflection/ReflectionSubsystem.h"
#include "Reflection/TypeDescriptor.h"

namespace
{
bool AllowsRead(const PropertyDescriptor& Descriptor, PropertyAccessContext Context)
{
    switch (Context)
    {
    case PropertyAccessContext::Script:
        return HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::ScriptReadable)
            || HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorVisible)
            || HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::Serializable);
    case PropertyAccessContext::Inspector:
        return HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorVisible)
            || HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorEditable)
            || HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorReadOnly);
    case PropertyAccessContext::Deserialize:
        return HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::Serializable);
    case PropertyAccessContext::Default:
    default:
        return true;
    }
}

bool AllowsWrite(const PropertyDescriptor& Descriptor, PropertyAccessContext Context)
{
    if (Descriptor.bReadOnly)
    {
        return false;
    }
    if (Descriptor.WriteObject == nullptr && Descriptor.WriteRaw == nullptr)
    {
        return false;
    }

    switch (Context)
    {
    case PropertyAccessContext::Script:
        return HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::ScriptWritable);
    case PropertyAccessContext::Inspector:
        return HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorEditable);
    case PropertyAccessContext::Deserialize:
        return HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::Serializable)
            && !HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::Transient);
    case PropertyAccessContext::Default:
    default:
        return true;
    }
}

ReflectionDiagnostic ValidateIncomingValue(const PropertyDescriptor& Descriptor, const ReflectedValue& InValue)
{
    if (Descriptor.ValueTypeId.IsValid())
    {
        if (TypeDescriptor* Type = ReflectionSubsystem::Get().FindType(Descriptor.ValueTypeId))
        {
            if (Type->Validate != nullptr)
            {
                ReflectionDiagnostic TypeValidation = Type->Validate(InValue, &Descriptor.Attributes);
                if (!TypeValidation.bOk)
                {
                    TypeValidation.Type = Descriptor.DeclaringTypeId;
                    TypeValidation.Property = Descriptor.Id;
                    return TypeValidation;
                }
            }
        }
    }

    if (Descriptor.Attributes.bHasRange)
    {
        double NumericValue = 0.0;
        bool bNumeric = false;
        if (InValue.ValueKind == ReflectedValue::Kind::Float64)
        {
            NumericValue = InValue.Float64Value;
            bNumeric = true;
        }
        else if (InValue.ValueKind == ReflectedValue::Kind::Int64)
        {
            NumericValue = static_cast<double>(InValue.Int64Value);
            bNumeric = true;
        }

        if (bNumeric)
        {
            if (NumericValue < Descriptor.Attributes.RangeMinimum || NumericValue > Descriptor.Attributes.RangeMaximum)
            {
                return ReflectionDiagnostic::Fail(
                    "Value out of range",
                    Descriptor.DeclaringTypeId,
                    Descriptor.Id);
            }
        }
    }

    return ReflectionDiagnostic::Ok();
}

const PropertyDescriptor* FindStructProperty(const TypeId& DeclaringTypeId, const PropertyId& Property)
{
    return ReflectionSubsystem::Get().FindStructProperty(DeclaringTypeId, Property);
}
}

ReflectionDiagnostic PropertyAccess::GetProperty(
    Object* Instance,
    const PropertyId& Property,
    ReflectedValue& OutValue,
    PropertyAccessContext Context)
{
    if (Instance == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is null");
    }

    Class* ObjectClass = Instance->GetClass();
    if (ObjectClass == nullptr)
    {
        return ReflectionDiagnostic::Fail("Object has no Class");
    }

    const PropertyDescriptor* Descriptor = ObjectClass->FindProperty(Property);
    if (Descriptor == nullptr)
    {
        return ReflectionDiagnostic::Fail("Property not found", ObjectClass->GetTypeId(), Property);
    }

    return GetProperty(Instance, *Descriptor, OutValue, Context);
}

ReflectionDiagnostic PropertyAccess::SetProperty(
    Object* Instance,
    const PropertyId& Property,
    const ReflectedValue& InValue,
    PropertyAccessContext Context)
{
    if (Instance == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is null");
    }

    Class* ObjectClass = Instance->GetClass();
    if (ObjectClass == nullptr)
    {
        return ReflectionDiagnostic::Fail("Object has no Class");
    }

    if (ObjectClass->GetClassDefaultObject() == Instance)
    {
        return ReflectionDiagnostic::Fail("Cannot mutate Class Default Object", ObjectClass->GetTypeId(), Property);
    }

    const PropertyDescriptor* Descriptor = ObjectClass->FindProperty(Property);
    if (Descriptor == nullptr)
    {
        return ReflectionDiagnostic::Fail("Property not found", ObjectClass->GetTypeId(), Property);
    }

    return SetProperty(Instance, *Descriptor, InValue, Context);
}

ReflectionDiagnostic PropertyAccess::GetProperty(
    void* Instance,
    const TypeId& DeclaringTypeId,
    const PropertyId& Property,
    ReflectedValue& OutValue,
    PropertyAccessContext Context)
{
    const PropertyDescriptor* Descriptor = FindStructProperty(DeclaringTypeId, Property);
    if (Descriptor == nullptr)
    {
        return ReflectionDiagnostic::Fail("Property not found", DeclaringTypeId, Property);
    }
    if (!AllowsRead(*Descriptor, Context))
    {
        return ReflectionDiagnostic::Fail("Property read not allowed in context", DeclaringTypeId, Property);
    }
    if (Descriptor->ReadRaw == nullptr)
    {
        return ReflectionDiagnostic::Fail("Property has no raw reader", DeclaringTypeId, Property);
    }
    return Descriptor->ReadRaw(Instance, OutValue);
}

ReflectionDiagnostic PropertyAccess::SetProperty(
    void* Instance,
    const TypeId& DeclaringTypeId,
    const PropertyId& Property,
    const ReflectedValue& InValue,
    PropertyAccessContext Context)
{
    const PropertyDescriptor* Descriptor = FindStructProperty(DeclaringTypeId, Property);
    if (Descriptor == nullptr)
    {
        return ReflectionDiagnostic::Fail("Property not found", DeclaringTypeId, Property);
    }
    if (!AllowsWrite(*Descriptor, Context))
    {
        return ReflectionDiagnostic::Fail("Property write not allowed in context", DeclaringTypeId, Property);
    }

    ReflectionDiagnostic Validated = ValidateIncomingValue(*Descriptor, InValue);
    if (!Validated.bOk)
    {
        return Validated;
    }
    if (Descriptor->WriteRaw == nullptr)
    {
        return ReflectionDiagnostic::Fail("Property has no raw writer", DeclaringTypeId, Property);
    }
    return Descriptor->WriteRaw(Instance, InValue);
}

ReflectionDiagnostic PropertyAccess::GetProperty(
    Object* Instance,
    const PropertyDescriptor& Descriptor,
    ReflectedValue& OutValue,
    PropertyAccessContext Context)
{
    if (!AllowsRead(Descriptor, Context))
    {
        return ReflectionDiagnostic::Fail("Property read not allowed in context", Descriptor.DeclaringTypeId, Descriptor.Id);
    }
    if (Descriptor.ReadObject == nullptr)
    {
        return ReflectionDiagnostic::Fail("Property has no object reader", Descriptor.DeclaringTypeId, Descriptor.Id);
    }
    return Descriptor.ReadObject(Instance, Descriptor, OutValue);
}

ReflectionDiagnostic PropertyAccess::SetProperty(
    Object* Instance,
    const PropertyDescriptor& Descriptor,
    const ReflectedValue& InValue,
    PropertyAccessContext Context)
{
    if (Instance != nullptr)
    {
        if (Class* ObjectClass = Instance->GetClass())
        {
            if (ObjectClass->GetClassDefaultObject() == Instance)
            {
                return ReflectionDiagnostic::Fail(
                    "Cannot mutate Class Default Object",
                    Descriptor.DeclaringTypeId,
                    Descriptor.Id);
            }
        }
    }

    if (!AllowsWrite(Descriptor, Context))
    {
        return ReflectionDiagnostic::Fail("Property write not allowed in context", Descriptor.DeclaringTypeId, Descriptor.Id);
    }

    ReflectionDiagnostic Validated = ValidateIncomingValue(Descriptor, InValue);
    if (!Validated.bOk)
    {
        return Validated;
    }

    if (Descriptor.WriteObject == nullptr)
    {
        return ReflectionDiagnostic::Fail("Property has no object writer", Descriptor.DeclaringTypeId, Descriptor.Id);
    }

    return Descriptor.WriteObject(Instance, Descriptor, InValue);
}
