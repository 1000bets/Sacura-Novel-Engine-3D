#pragma once

#include "Reflection/ReflectedValue.h"
#include "Reflection/TypeId.h"

#include <SimpleMath.h>
#include <cstring>
#include <string>

using namespace DirectX::SimpleMath;

inline TypeId MakeBuiltinTypeId(const char* Value)
{
    return TypeId{Value};
}

template <typename ValueType>
struct ReflectionValueCodec;

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
struct ReflectionValueCodec<Color>
{
    static TypeId GetTypeId() { return MakeBuiltinTypeId("engine.Color"); }

    static ReflectionDiagnostic ToReflected(const Color& Source, ReflectedValue& OutValue)
    {
        OutValue = ReflectedValue::MakeEmpty();
        OutValue.ValueKind = ReflectedValue::Kind::Bytes;
        OutValue.BytesValue.resize(sizeof(float) * 4);
        float Values[4] = {Source.x, Source.y, Source.z, Source.w};
        std::memcpy(OutValue.BytesValue.data(), Values, sizeof(Values));
        return ReflectionDiagnostic::Ok();
    }

    static ReflectionDiagnostic FromReflected(Color& Destination, const ReflectedValue& InValue)
    {
        if (InValue.ValueKind != ReflectedValue::Kind::Bytes || InValue.BytesValue.size() != sizeof(float) * 4)
        {
            return ReflectionDiagnostic::Fail("Expected Color bytes");
        }
        float Values[4] = {};
        std::memcpy(Values, InValue.BytesValue.data(), sizeof(Values));
        Destination = Color(Values[0], Values[1], Values[2], Values[3]);
        return ReflectionDiagnostic::Ok();
    }
};
