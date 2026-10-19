#include "Reflection/ReflectedValue.h"
#include "Reflection/ReflectionValueCodec.h"
#include "Reflection/ReflectionSubsystem.h"
#include "Reflection/PropertyAccess.h"
#include "Core/Object/MemorySubsystem.h"

ReflectionDiagnostic ReadReflectionStruct(const TypeId& Type, void* Instance, ReflectedValue& OutValue)
{
    const auto* Properties = ReflectionSubsystem::Get().FindStructProperties(Type);
    if (Properties == nullptr)
    {
        return ReflectionDiagnostic::Fail("Reflected struct fields are not registered", Type);
    }
    ReflectedValue Result;
    Result.ValueKind = ReflectedValue::Kind::Struct;
    Result.TypeIdValue = Type;
    for (const PropertyDescriptor& Property : *Properties)
    {
        if (!HasPropertyFlag(Property.Attributes.Flags, PropertyFlags::Serializable))
        {
            continue;
        }
        ReflectedValue Value;
        ReflectionDiagnostic Converted = Property.ReadRaw(Instance, Value);
        if (!Converted.bOk)
        {
            return Converted;
        }
        Result.FieldNames.push_back(Property.Id.Value);
        Result.Values.push_back(std::move(Value));
    }
    OutValue = std::move(Result);
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic WriteReflectionStruct(const TypeId& Type, void* Instance, const ReflectedValue& InValue)
{
    if (InValue.ValueKind != ReflectedValue::Kind::Struct || InValue.TypeIdValue != Type
        || InValue.FieldNames.size() != InValue.Values.size())
    {
        return ReflectionDiagnostic::Fail("Expected matching reflected struct", Type);
    }
    ReflectionSubsystem& Reflection = ReflectionSubsystem::Get();
    if (const TypeDescriptor* Descriptor = Reflection.FindType(Type))
    {
        if (Descriptor->Validate != nullptr)
        {
            ReflectionDiagnostic Validated = Descriptor->Validate(InValue, nullptr);
            if (!Validated.bOk)
            {
                return Validated;
            }
        }
    }
    for (size_t FieldIndex = 0; FieldIndex < InValue.Values.size(); ++FieldIndex)
    {
        ReflectionDiagnostic Converted = PropertyAccess::SetProperty(
            Instance, Type, PropertyId{InValue.FieldNames[FieldIndex]}, InValue.Values[FieldIndex],
            PropertyAccessContext::Deserialize);
        if (!Converted.bOk)
        {
            return Converted;
        }
    }
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReadReflectionObject(ObjectHandle Value, ReflectedValue& OutValue)
{
    OutValue = {};
    OutValue.ValueKind = ReflectedValue::Kind::ObjectReference;
    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (Memory != nullptr)
    {
        if (Object* Instance = Memory->ResolveHandle(Value))
        {
            OutValue.ObjectValue = Value;
            OutValue.StringValue = Instance->GetPersistentId();
        }
    }
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic WriteReflectionObject(ObjectHandle& Value, const ReflectedValue& InValue)
{
    if (InValue.ValueKind != ReflectedValue::Kind::ObjectReference)
    {
        return ReflectionDiagnostic::Fail("Expected object reference");
    }
    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (InValue.ObjectValue.IsValid() && (Memory == nullptr || Memory->ResolveHandle(InValue.ObjectValue) == nullptr))
    {
        return ReflectionDiagnostic::Fail("Object reference is no longer alive");
    }
    Value = InValue.ObjectValue;
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ValidateReflectionClass(const TypeId& Type, const TypeId& BaseType)
{
    if (!Type.IsValid())
    {
        return ReflectionDiagnostic::Ok();
    }
    Class* Candidate = ReflectionSubsystem::Get().FindClass(Type);
    if (Candidate == nullptr || !Candidate->IsA(BaseType))
    {
        return ReflectionDiagnostic::Fail("Class reference does not match its base type", Type);
    }
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ValidateReflectionEnum(const TypeId& Type, int64_t Value)
{
    if (const EnumDescriptor* Descriptor = ReflectionSubsystem::Get().FindEnum(Type))
    {
        for (const EnumValueDescriptor& Entry : Descriptor->Values)
        {
            if (Entry.Value == Value)
            {
                return ReflectionDiagnostic::Ok();
            }
        }
    }
    return ReflectionDiagnostic::Fail("Unknown reflected enum value", Type);
}

TypeId RegisterReflectionArrayType(const TypeId& ElementType)
{
    TypeDescriptor Descriptor;
    Descriptor.Id = TypeId{"engine.Array<" + ElementType.Value + ">"};
    Descriptor.Kind = TypeKind::Array;
    Descriptor.ElementTypeId = ElementType;
    TypeId Result = Descriptor.Id;
    ReflectionSubsystem::Get().RegisterTypeDescriptor(std::move(Descriptor));
    return Result;
}

ReflectionDiagnostic MakeDefaultReflectionValue(const TypeId& Type, ReflectedValue& OutValue)
{
    if (const TypeDescriptor* Descriptor = ReflectionSubsystem::Get().FindType(Type))
    {
        if (Descriptor->MakeDefault != nullptr)
        {
            return Descriptor->MakeDefault(OutValue);
        }
        if (Descriptor->Kind == TypeKind::Array)
        {
            OutValue = {};
            OutValue.ValueKind = ReflectedValue::Kind::Array;
            OutValue.TypeIdValue = Descriptor->ElementTypeId;
            return ReflectionDiagnostic::Ok();
        }
    }
    if (const EnumDescriptor* Descriptor = ReflectionSubsystem::Get().FindEnum(Type))
    {
        if (!Descriptor->Values.empty())
        {
            OutValue = ReflectedValue::MakeInt64(Descriptor->Values.front().Value);
            OutValue.ValueKind = ReflectedValue::Kind::Enum;
            OutValue.TypeIdValue = Type;
            return ReflectionDiagnostic::Ok();
        }
    }
    if (Type.Value.find("engine.ClassRef<") == 0)
    {
        OutValue = ReflectedValue::MakeTypeId({});
        return ReflectionDiagnostic::Ok();
    }
    return ReflectionDiagnostic::Fail("No default value factory for reflected type", Type);
}

TypeId RegisterReflectionClassType(const TypeId& BaseType)
{
    TypeDescriptor Descriptor;
    Descriptor.Id = TypeId{"engine.ClassRef<" + BaseType.Value + ">"};
    Descriptor.Kind = TypeKind::Reference;
    Descriptor.ReferencedTypeId = BaseType;
    TypeId Result = Descriptor.Id;
    ReflectionSubsystem::Get().RegisterTypeDescriptor(std::move(Descriptor));
    return Result;
}

ReflectedValue ReflectedValue::MakeBool(bool Value)
{
    ReflectedValue Result;
    Result.ValueKind = Kind::Bool;
    Result.BoolValue = Value;
    return Result;
}

ReflectedValue ReflectedValue::MakeInt64(int64_t Value)
{
    ReflectedValue Result;
    Result.ValueKind = Kind::Int64;
    Result.Int64Value = Value;
    return Result;
}

ReflectedValue ReflectedValue::MakeFloat64(double Value)
{
    ReflectedValue Result;
    Result.ValueKind = Kind::Float64;
    Result.Float64Value = Value;
    return Result;
}

ReflectedValue ReflectedValue::MakeFloat(float Value)
{
    return MakeFloat64(static_cast<double>(Value));
}

ReflectedValue ReflectedValue::MakeString(std::string Value)
{
    ReflectedValue Result;
    Result.ValueKind = Kind::String;
    Result.StringValue = std::move(Value);
    return Result;
}

ReflectedValue ReflectedValue::MakeTypeId(TypeId Value)
{
    ReflectedValue Result;
    Result.ValueKind = Kind::TypeId;
    Result.TypeIdValue = std::move(Value);
    return Result;
}

ReflectedValue ReflectedValue::MakeObjectId(uint64_t Value)
{
    ReflectedValue Result;
    Result.ValueKind = Kind::ObjectId;
    Result.ObjectIdValue = Value;
    return Result;
}

ReflectionDiagnostic ReflectionDiagnostic::Ok()
{
    ReflectionDiagnostic Diagnostic;
    Diagnostic.bOk = true;
    return Diagnostic;
}

ReflectionDiagnostic ReflectionDiagnostic::Fail(std::string Message, TypeId Type, PropertyId Property)
{
    ReflectionDiagnostic Diagnostic;
    Diagnostic.bOk = false;
    Diagnostic.Message = std::move(Message);
    Diagnostic.Type = std::move(Type);
    Diagnostic.Property = std::move(Property);
    return Diagnostic;
}
