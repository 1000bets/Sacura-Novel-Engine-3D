#include "Reflection/ReflectedValue.h"

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
