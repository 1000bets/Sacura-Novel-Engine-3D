#include "Reflection/PropertyAccess.h"

#include "Core/Object/Object.h"
#include "Reflection/Class.h"
#include "Reflection/ReflectionSubsystem.h"
#include "Reflection/TypeDescriptor.h"
#include "Reflection/ReflectionValueCodec.h"
#include <cmath>
#include <unordered_set>

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

ReflectionDiagnostic ValidateIncomingValue(const PropertyDescriptor& Descriptor, const ReflectedValue& InValue);

ReflectionDiagnostic ValidateValueType(const TypeId& Type, const ReflectedValue& Value)
{
    ReflectionSubsystem& Reflection = ReflectionSubsystem::Get();
    if (Value.ValueKind == ReflectedValue::Kind::Struct)
    {
        const TypeDescriptor* StructType = Reflection.FindType(Type);
        if (StructType == nullptr || StructType->Kind != TypeKind::Struct
            || Value.TypeIdValue != Type || Value.FieldNames.size() != Value.Values.size())
        {
            return ReflectionDiagnostic::Fail("Struct type mismatch", Type);
        }
        std::unordered_set<std::string> SeenFields;
        for (size_t FieldIndex = 0; FieldIndex < Value.Values.size(); ++FieldIndex)
        {
            const PropertyDescriptor* Field = Reflection.FindStructProperty(Type, PropertyId{Value.FieldNames[FieldIndex]});
            if (Field == nullptr || !SeenFields.insert(Field->Id.Value).second)
            {
                return ReflectionDiagnostic::Fail("Unknown or duplicate struct field", Type);
            }
            ReflectionDiagnostic Validated = ValidateIncomingValue(*Field, Value.Values[FieldIndex]);
            if (!Validated.bOk)
            {
                return Validated;
            }
        }
        return ReflectionDiagnostic::Ok();
    }
    if (Value.ValueKind == ReflectedValue::Kind::Array)
    {
        const TypeDescriptor* Descriptor = Reflection.FindType(Type);
        if (Descriptor == nullptr || Descriptor->Kind != TypeKind::Array || Descriptor->ElementTypeId != Value.TypeIdValue)
        {
            return ReflectionDiagnostic::Fail("Array type mismatch", Type);
        }
        std::unordered_map<std::string, std::unordered_set<int64_t>> CollectionIndices;
        for (const ReflectedValue& Element : Value.Values)
        {
            ReflectionDiagnostic Validated = ValidateValueType(Value.TypeIdValue, Element);
            if (!Validated.bOk)
            {
                return Validated;
            }
            if (const auto* Fields = Reflection.FindStructProperties(Value.TypeIdValue))
            {
                for (const PropertyDescriptor& Field : *Fields)
                {
                    if (Field.Attributes.bCollectionIndex)
                    {
                        const ReflectedValue* Index = Element.FindField(Field.Id.Value);
                        if (Index == nullptr || Index->ValueKind != ReflectedValue::Kind::Int64
                            || !CollectionIndices[Field.Id.Value].insert(Index->Int64Value).second)
                        {
                            return ReflectionDiagnostic::Fail("Missing or duplicate collection index", Type, Field.Id);
                        }
                    }
                }
            }
        }
        return ReflectionDiagnostic::Ok();
    }
    if (Reflection.FindEnum(Type) != nullptr)
    {
        if (Value.ValueKind != ReflectedValue::Kind::Enum || Value.TypeIdValue != Type)
        {
            return ReflectionDiagnostic::Fail("Enum type mismatch", Type);
        }
        return ValidateReflectionEnum(Type, Value.Int64Value);
    }
    const std::string ClassPrefix = "engine.ClassRef<";
    if (Type.Value.find(ClassPrefix) == 0)
    {
        if (Value.ValueKind != ReflectedValue::Kind::TypeId)
        {
            return ReflectionDiagnostic::Fail("Expected class reference", Type);
        }
        return ValidateReflectionClass(Value.TypeIdValue,
            TypeId{Type.Value.substr(ClassPrefix.size(), Type.Value.size() - ClassPrefix.size() - 1)});
    }
    if (Type.Value == "engine.ObjectHandle")
    {
        ObjectHandle Target;
        return WriteReflectionObject(Target, Value);
    }
    const bool bValid = (Type.Value == "engine.bool" && Value.ValueKind == ReflectedValue::Kind::Bool)
        || (Type.Value == "engine.int64" && Value.ValueKind == ReflectedValue::Kind::Int64)
        || ((Type.Value == "engine.float" || Type.Value == "engine.double")
            && Value.ValueKind == ReflectedValue::Kind::Float64 && std::isfinite(Value.Float64Value))
        || (Type.Value == "engine.string" && Value.ValueKind == ReflectedValue::Kind::String)
        || (Type.Value == "engine.TypeId" && Value.ValueKind == ReflectedValue::Kind::TypeId)
        || (Type.Value == "engine.AssetRef" && Value.ValueKind == ReflectedValue::Kind::AssetReference)
        || ((Type.Value == "engine.Vector3" || Type.Value == "engine.EulerAngles")
            && Value.ValueKind == ReflectedValue::Kind::Bytes && Value.BytesValue.size() == sizeof(float) * 3)
        || (Type.Value == "engine.Quaternion" && Value.ValueKind == ReflectedValue::Kind::Bytes
            && Value.BytesValue.size() == sizeof(float) * 4);
    if (!bValid)
    {
        return ReflectionDiagnostic::Fail("Reflected value type mismatch", Type);
    }
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ValidateIncomingValue(const PropertyDescriptor& Descriptor, const ReflectedValue& InValue)
{
    ReflectionDiagnostic ValueValidation = ValidateValueType(Descriptor.ValueTypeId, InValue);
    if (!ValueValidation.bOk)
    {
        return ValueValidation;
    }
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

ReflectionDiagnostic PropertyAccess::ValidatePropertyValue(const PropertyDescriptor& Descriptor, const ReflectedValue& Value)
{
    return ValidateIncomingValue(Descriptor, Value);
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

ReflectionDiagnostic PropertyAccess::CopyPropertiesFrom(
    Object* Source,
    Object* Destination,
    PropertyAccessContext Context)
{
    if (Source == nullptr || Destination == nullptr)
    {
        return ReflectionDiagnostic::Fail("Source or Destination is null");
    }

    Class* SourceClass = Source->GetClass();
    Class* DestinationClass = Destination->GetClass();
    if (SourceClass == nullptr || DestinationClass == nullptr)
    {
        return ReflectionDiagnostic::Fail("Source or Destination has no Class");
    }

    if (!DestinationClass->IsA(SourceClass) && !SourceClass->IsA(DestinationClass) && SourceClass != DestinationClass)
    {
        return ReflectionDiagnostic::Fail("Incompatible Class hierarchy for property copy");
    }

    for (Class* Current = SourceClass; Current != nullptr; Current = Current->GetBaseClass())
    {
        for (const PropertyDescriptor& Descriptor : Current->GetProperties())
        {
            ReflectedValue Value;
            ReflectionDiagnostic GetResult = GetProperty(Source, Descriptor, Value, Context);
            if (!GetResult.bOk)
            {
                continue;
            }

            ReflectionDiagnostic SetResult = SetProperty(Destination, Descriptor, Value, Context);
            if (!SetResult.bOk)
            {
                return SetResult;
            }
        }
    }

    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic PropertyAccess::ResetToClassDefaults(
    Object* Instance,
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

    Object* Defaults = ObjectClass->GetClassDefaultObject();
    if (Defaults == nullptr)
    {
        return ReflectionDiagnostic::Fail("Class has no CDO", ObjectClass->GetTypeId());
    }

    if (Defaults == Instance)
    {
        return ReflectionDiagnostic::Fail("Cannot reset CDO onto itself", ObjectClass->GetTypeId());
    }

    return CopyPropertiesFrom(Defaults, Instance, Context);
}
