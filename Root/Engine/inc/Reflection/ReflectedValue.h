#pragma once

#include "Reflection/TypeId.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

class Object;

// Owning small value used at reflection/script/JSON boundaries (copy semantics).
struct ReflectedValue
{
    enum class Kind : uint8_t
    {
        Empty = 0,
        Bool,
        Int64,
        Float64,
        String,
        TypeId,
        ObjectId,
        Bytes
    };

    Kind ValueKind = Kind::Empty;
    bool BoolValue = false;
    int64_t Int64Value = 0;
    double Float64Value = 0.0;
    std::string StringValue;
    TypeId TypeIdValue{};
    uint64_t ObjectIdValue = 0;
    std::vector<uint8_t> BytesValue;

    static ReflectedValue MakeEmpty() { return {}; }
    static ReflectedValue MakeBool(bool Value);
    static ReflectedValue MakeInt64(int64_t Value);
    static ReflectedValue MakeFloat64(double Value);
    static ReflectedValue MakeFloat(float Value);
    static ReflectedValue MakeString(std::string Value);
    static ReflectedValue MakeTypeId(TypeId Value);
    static ReflectedValue MakeObjectId(uint64_t Value);

    bool IsEmpty() const { return ValueKind == Kind::Empty; }
};

struct ReflectionDiagnostic
{
    bool bOk = true;
    std::string Message;
    TypeId Type{};
    PropertyId Property{};

    static ReflectionDiagnostic Ok();
    static ReflectionDiagnostic Fail(std::string Message, TypeId Type = {}, PropertyId Property = {});
};
