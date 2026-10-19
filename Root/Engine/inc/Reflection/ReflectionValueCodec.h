#pragma once

#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"
#include "Reflection/EulerAngles.h"
#include "Reflection/ClassRef.h"
#include "Assets/AssetRef.h"

#include <SimpleMath.h>
#include <cstring>
#include <string>
#include <type_traits>

using namespace DirectX::SimpleMath;

inline TypeId MakeBuiltinTypeId(const char* Value)
{
    return TypeId{Value};
}

ReflectionDiagnostic ReadReflectionStruct(const TypeId& Type, void* Instance, ReflectedValue& OutValue);
ReflectionDiagnostic WriteReflectionStruct(const TypeId& Type, void* Instance, const ReflectedValue& InValue);
ReflectionDiagnostic ReadReflectionObject(ObjectHandle Value, ReflectedValue& OutValue);
ReflectionDiagnostic WriteReflectionObject(ObjectHandle& Value, const ReflectedValue& InValue);
ReflectionDiagnostic ValidateReflectionClass(const TypeId& Type, const TypeId& BaseType);
ReflectionDiagnostic ValidateReflectionEnum(const TypeId& Type, int64_t Value);
TypeId RegisterReflectionArrayType(const TypeId& ElementType);
TypeId RegisterReflectionClassType(const TypeId& BaseType);
ReflectionDiagnostic MakeDefaultReflectionValue(const TypeId& Type, ReflectedValue& OutValue);

template <typename ValueType>
struct ReflectionStructType
{
    static TypeId GetTypeId()
    {
        return TypeId{ValueType::StaticReflectionTypeId()};
    }
};

template <>
struct ReflectionStructType<Color>
{
    static TypeId GetTypeId()
    {
        return TypeId{"engine.Color"};
    }
};

template <typename EnumType>
struct ReflectionEnumType;

template <typename ValueType, typename Enable = void>
struct ReflectionValueCodec
{
    static TypeId GetTypeId()
    {
        return ReflectionStructType<ValueType>::GetTypeId();
    }

    static ReflectionDiagnostic ToReflected(const ValueType& Source, ReflectedValue& OutValue)
    {
        return ReadReflectionStruct(GetTypeId(), const_cast<ValueType*>(&Source), OutValue);
    }

    static ReflectionDiagnostic FromReflected(ValueType& Destination, const ReflectedValue& InValue)
    {
        ValueType Candidate = Destination;
        ReflectionDiagnostic Converted = WriteReflectionStruct(GetTypeId(), &Candidate, InValue);
        if (Converted.bOk)
        {
            Destination = std::move(Candidate);
        }
        return Converted;
    }
};

template <>
struct ReflectionValueCodec<bool>
{
    static TypeId GetTypeId() { return MakeBuiltinTypeId("engine.bool"); }

    static ReflectionDiagnostic ToReflected(const bool& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeBool(Source);
        return ReflectionDiagnostic::Ok();
    }

    static ReflectionDiagnostic FromReflected(bool& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Bool)
        {
            return ReflectionDiagnostic::Fail("Expected bool");
        }
        Destination = InValue.BoolValue;
        return ReflectionDiagnostic::Ok();
    }
};

template <>
struct ReflectionValueCodec<int64_t>
{
    static TypeId GetTypeId() { return MakeBuiltinTypeId("engine.int64"); }

    static ReflectionDiagnostic ToReflected(const int64_t& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeInt64(Source);
        return ReflectionDiagnostic::Ok();
    }

    static ReflectionDiagnostic FromReflected(int64_t& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Int64)
        {
            return ReflectionDiagnostic::Fail("Expected int64");
        }
        Destination = InValue.Int64Value;
        return ReflectionDiagnostic::Ok();
    }
};

template <>
struct ReflectionValueCodec<int>
{
    static TypeId GetTypeId() { return MakeBuiltinTypeId("engine.int64"); }

    static ReflectionDiagnostic ToReflected(const int& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeInt64(Source);
        return ReflectionDiagnostic::Ok();
    }

    static ReflectionDiagnostic FromReflected(int& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Int64)
        {
            return ReflectionDiagnostic::Fail("Expected int64");
        }
        Destination = static_cast<int>(InValue.Int64Value);
        return ReflectionDiagnostic::Ok();
    }
};

template <>
struct ReflectionValueCodec<float>
{
    static TypeId GetTypeId() { return MakeBuiltinTypeId("engine.float"); }

    static ReflectionDiagnostic ToReflected(const float& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeFloat(Source);
        return ReflectionDiagnostic::Ok();
    }

    static ReflectionDiagnostic FromReflected(float& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Float64)
        {
            return ReflectionDiagnostic::Fail("Expected float");
        }
        Destination = static_cast<float>(InValue.Float64Value);
        return ReflectionDiagnostic::Ok();
    }
};

template <>
struct ReflectionValueCodec<double>
{
    static TypeId GetTypeId() { return MakeBuiltinTypeId("engine.double"); }

    static ReflectionDiagnostic ToReflected(const double& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeFloat64(Source);
        return ReflectionDiagnostic::Ok();
    }

    static ReflectionDiagnostic FromReflected(double& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Float64)
        {
            return ReflectionDiagnostic::Fail("Expected double");
        }
        Destination = InValue.Float64Value;
        return ReflectionDiagnostic::Ok();
    }
};

template <>
struct ReflectionValueCodec<std::string>
{
    static TypeId GetTypeId() { return MakeBuiltinTypeId("engine.string"); }

    static ReflectionDiagnostic ToReflected(const std::string& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeString(Source);
        return ReflectionDiagnostic::Ok();
    }

    static ReflectionDiagnostic FromReflected(std::string& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::String)
        {
            return ReflectionDiagnostic::Fail("Expected string");
        }
        Destination = InValue.StringValue;
        return ReflectionDiagnostic::Ok();
    }
};

template <>
struct ReflectionValueCodec<Vector3>
{
    static TypeId GetTypeId() { return MakeBuiltinTypeId("engine.Vector3"); }

    static ReflectionDiagnostic ToReflected(const Vector3& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeEmpty();
        OutValue.ValueKind = ReflectedValue::Kind::Bytes;
        OutValue.BytesValue.resize(sizeof(float) * 3);
        float Values[3] = {Source.x, Source.y, Source.z};
        std::memcpy(OutValue.BytesValue.data(), Values, sizeof(Values));
        return ReflectionDiagnostic::Ok();
    }

    static ReflectionDiagnostic FromReflected(Vector3& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Bytes || InValue.BytesValue.size() != sizeof(float) * 3)
        {
            return ReflectionDiagnostic::Fail("Expected Vector3 bytes");
        }
        float Values[3] = {};
        std::memcpy(Values, InValue.BytesValue.data(), sizeof(Values));
        Destination = Vector3(Values[0], Values[1], Values[2]);
        return ReflectionDiagnostic::Ok();
    }
};

template <>
struct ReflectionValueCodec<Quaternion>
{
    static TypeId GetTypeId() { return MakeBuiltinTypeId("engine.Quaternion"); }

    static ReflectionDiagnostic ToReflected(const Quaternion& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeEmpty();
        OutValue.ValueKind = ReflectedValue::Kind::Bytes;
        OutValue.BytesValue.resize(sizeof(float) * 4);
        float Values[4] = {Source.x, Source.y, Source.z, Source.w};
        std::memcpy(OutValue.BytesValue.data(), Values, sizeof(Values));
        return ReflectionDiagnostic::Ok();
    }

    static ReflectionDiagnostic FromReflected(Quaternion& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Bytes || InValue.BytesValue.size() != sizeof(float) * 4)
        {
            return ReflectionDiagnostic::Fail("Expected Quaternion bytes");
        }
        float Values[4] = {};
        std::memcpy(Values, InValue.BytesValue.data(), sizeof(Values));
        Destination = Quaternion(Values[0], Values[1], Values[2], Values[3]);
        return ReflectionDiagnostic::Ok();
    }
};

template <>
struct ReflectionValueCodec<EulerAngles>
{
    static TypeId GetTypeId() { return TypeId{"engine.EulerAngles"}; }

    static ReflectionDiagnostic ToReflected(const EulerAngles& Source, ReflectedValue& OutValue)
    {
        ReflectionDiagnostic Converted = ReflectionValueCodec<Vector3>::ToReflected(Vector3(Source.Pitch, Source.Yaw, Source.Roll), OutValue);
        OutValue.TypeIdValue = GetTypeId();
        return Converted;
    }

    static ReflectionDiagnostic FromReflected(EulerAngles& Destination, const ReflectedValue& InValue)
    {
        Vector3 Angles;
        ReflectionDiagnostic Converted = ReflectionValueCodec<Vector3>::FromReflected(Angles, InValue);
        if (Converted.bOk)
        {
            Destination = {Angles.x, Angles.y, Angles.z};
        }
        return Converted;
    }
};

template <>
struct ReflectionValueCodec<TypeId>
{
    static TypeId GetTypeId() { return TypeId{"engine.TypeId"}; }
    static ReflectionDiagnostic ToReflected(const TypeId& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeTypeId(Source);
        return ReflectionDiagnostic::Ok();
    }
    static ReflectionDiagnostic FromReflected(TypeId& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::TypeId)
        {
            return ReflectionDiagnostic::Fail("Expected TypeId");
        }
        Destination = InValue.TypeIdValue;
        return ReflectionDiagnostic::Ok();
    }
};

template <>
struct ReflectionValueCodec<ObjectHandle>
{
    static TypeId GetTypeId() { return TypeId{"engine.ObjectHandle"}; }
    static ReflectionDiagnostic ToReflected(const ObjectHandle& Source, ReflectedValue& OutValue)
    {
        return ReadReflectionObject(Source, OutValue);
    }
    static ReflectionDiagnostic FromReflected(ObjectHandle& Destination, const ReflectedValue& InValue)
    {
        return WriteReflectionObject(Destination, InValue);
    }
};

template <typename BaseType>
struct ReflectionValueCodec<ClassRef<BaseType>>
{
    static TypeId GetTypeId()
    {
        return RegisterReflectionClassType(TypeId{BaseType::StaticReflectionTypeId()});
    }
    static ReflectionDiagnostic ToReflected(const ClassRef<BaseType>& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeTypeId(Source.Id);
        return ReflectionDiagnostic::Ok();
    }
    static ReflectionDiagnostic FromReflected(ClassRef<BaseType>& Destination, const ReflectedValue& InValue)
    {
        TypeId Candidate;
        ReflectionDiagnostic Converted = ReflectionValueCodec<TypeId>::FromReflected(Candidate, InValue);
        if (!Converted.bOk)
        {
            return Converted;
        }
        ReflectionDiagnostic Validated = ValidateReflectionClass(Candidate, TypeId{BaseType::StaticReflectionTypeId()});
        if (Validated.bOk)
        {
            Destination.Id = std::move(Candidate);
        }
        return Validated;
    }
};

template <>
struct ReflectionValueCodec<AssetKey>
{
    static TypeId GetTypeId() { return TypeId{"engine.AssetRef"}; }
    static ReflectionDiagnostic ToReflected(const AssetKey& Source, ReflectedValue& OutValue)
    {
        OutValue = {};
        OutValue.ValueKind = ReflectedValue::Kind::AssetReference;
        OutValue.AssetValue = Source;
        return ReflectionDiagnostic::Ok();
    }
    static ReflectionDiagnostic FromReflected(AssetKey& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::AssetReference)
        {
            return ReflectionDiagnostic::Fail("Expected asset reference");
        }
        Destination = InValue.AssetValue;
        return ReflectionDiagnostic::Ok();
    }
};

template <typename ResourceType>
struct ReflectionValueCodec<AssetRef<ResourceType>>
{
    static TypeId GetTypeId() { return ReflectionValueCodec<AssetKey>::GetTypeId(); }
    static ReflectionDiagnostic ToReflected(const AssetRef<ResourceType>& Source, ReflectedValue& OutValue)
    {
        return ReflectionValueCodec<AssetKey>::ToReflected(Source.Key, OutValue);
    }
    static ReflectionDiagnostic FromReflected(AssetRef<ResourceType>& Destination, const ReflectedValue& InValue)
    {
        return ReflectionValueCodec<AssetKey>::FromReflected(Destination.Key, InValue);
    }
};

template <typename ElementType>
struct ReflectionValueCodec<std::vector<ElementType>>
{
    static TypeId GetTypeId()
    {
        return RegisterReflectionArrayType(ReflectionValueCodec<ElementType>::GetTypeId());
    }
    static ReflectionDiagnostic ToReflected(const std::vector<ElementType>& Source, ReflectedValue& OutValue)
    {
        OutValue = {};
        OutValue.ValueKind = ReflectedValue::Kind::Array;
        OutValue.TypeIdValue = ReflectionValueCodec<ElementType>::GetTypeId();
        OutValue.Values.reserve(Source.size());
        for (const ElementType& Element : Source)
        {
            ReflectedValue Converted;
            ReflectionDiagnostic Result = ReflectionValueCodec<ElementType>::ToReflected(Element, Converted);
            if (!Result.bOk)
            {
                return Result;
            }
            OutValue.Values.push_back(std::move(Converted));
        }
        return ReflectionDiagnostic::Ok();
    }
    static ReflectionDiagnostic FromReflected(std::vector<ElementType>& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Array
            || InValue.TypeIdValue != ReflectionValueCodec<ElementType>::GetTypeId())
        {
            return ReflectionDiagnostic::Fail("Expected reflected array");
        }
        std::vector<ElementType> Candidate;
        Candidate.reserve(InValue.Values.size());
        for (const ReflectedValue& Element : InValue.Values)
        {
            ElementType Converted{};
            ReflectionDiagnostic Result = ReflectionValueCodec<ElementType>::FromReflected(Converted, Element);
            if (!Result.bOk)
            {
                return Result;
            }
            Candidate.push_back(std::move(Converted));
        }
        Destination = std::move(Candidate);
        return ReflectionDiagnostic::Ok();
    }
};

template <typename EnumType>
struct ReflectionValueCodec<EnumType, std::enable_if_t<std::is_enum_v<EnumType>>>
{
    static TypeId GetTypeId() { return ReflectionEnumType<EnumType>::GetTypeId(); }
    static ReflectionDiagnostic ToReflected(const EnumType& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeInt64(static_cast<int64_t>(Source));
        OutValue.ValueKind = ReflectedValue::Kind::Enum;
        OutValue.TypeIdValue = GetTypeId();
        return ValidateReflectionEnum(GetTypeId(), OutValue.Int64Value);
    }
    static ReflectionDiagnostic FromReflected(EnumType& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Enum || InValue.TypeIdValue != GetTypeId())
        {
            return ReflectionDiagnostic::Fail("Expected reflected enum");
        }
        ReflectionDiagnostic Validated = ValidateReflectionEnum(GetTypeId(), InValue.Int64Value);
        if (Validated.bOk)
        {
            Destination = static_cast<EnumType>(InValue.Int64Value);
        }
        return Validated;
    }
};
